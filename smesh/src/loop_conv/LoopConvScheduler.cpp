// **********************************************************************
// smesh/src/loop_conv/LoopConvScheduler.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvScheduler.hpp"
#include "SmeshTypes.hpp"

#include <cassert>

namespace smesh {
namespace {

constexpr std::uint64_t mask(unsigned width) {
  return (std::uint64_t{1} << width) - 1;
}

// Original's 16-bit kernel operands produce a 32-bit dilated-kernel extent.
std::uint32_t dilatedKernel(std::uint16_t size, std::uint16_t dilation) {
  const auto extra = std::uint64_t{static_cast<std::uint16_t>(dilation - 1)} *
                     static_cast<std::uint16_t>(size - 1);
  return static_cast<std::uint32_t>(size + extra);
}

struct InputExtent {
  std::uint16_t padded;
  std::uint16_t unpadded;
};

// Preserve +&/-& widths (33..36 bits), then truncate at the result fields.
InputExtent inputExtent(std::uint16_t outputs, std::uint16_t stride,
                        std::uint16_t kernel, std::uint16_t dilation,
                        std::uint16_t before, std::uint16_t after, bool input_dilated) {
  const auto sum = (std::uint64_t{outputs} * stride + dilatedKernel(kernel, dilation)) & mask(33);
  const auto extent = (sum - 1) & mask(34);
  const auto first_subtraction = (extent - before) & mask(35);
  const auto without_padding = (first_subtraction - after) & mask(36);
  const auto unpadded = (without_padding + unsigned(input_dilated)) >> unsigned(input_dilated);
  const auto padded = input_dilated
      ? unpadded + ((std::uint32_t{before} + 1) >> 1) + ((std::uint32_t{after} + 1) >> 1)
      : extent;
  return {static_cast<std::uint16_t>(padded), static_cast<std::uint16_t>(unpadded)};
}

// Strides use the already-truncated dimensions, as in Original's derived bundle.
LoopConvInputGeometry inputGeometry(const LoopConvSlot& slot) {
  const auto& outer  = slot.outer_bounds;
  const auto& inner  = slot.inner_bounds;
  const bool dilated = slot.launch.input_dilated == 1;
  const auto rows    = inputExtent(inner.orows, outer.stride, inner.krows,
                                  outer.kernel_dilation, inner.upad, inner.dpad, dilated);
  const auto cols    = inputExtent(inner.ocols, outer.stride, inner.kcols,
                                  outer.kernel_dilation, inner.lpad, inner.rpad, dilated);
  LoopConvInputGeometry result{};
  result.irows          = rows.padded;
  result.icols          = cols.padded;
  result.irows_unpadded = rows.unpadded;
  result.icols_unpadded = cols.unpadded;
  result.ichs           = inner.kchs;
  const auto planes     = slot.launch.trans_input_3120 == 1 ? result.ichs : inner.batches;
  const unsigned shift  = slot.launch.downsample == 1 ? 1 : 0;
  result.input_spad_stride = static_cast<std::uint16_t>(std::uint64_t{planes} * (result.irows >> shift) * (result.icols >> shift));
  return result;
}

} // namespace

LoopConvScheduler::LoopConvScheduler(std::string /*name*/, IMPL_CTOR) {
  ld_bias_addr_start_Q_ <= ld_bias_addr_start_D_;
  UPDATE(updateRequest)
      .reads(loop0, loop1, head_loop_id, ld_bias_addr_start_Q_)
      .writes(ld_bias_req_val, ld_bias_req_bits);
  UPDATE(updateTransfer)
      .reads(ld_bias_req_val, ld_bias_req_bits, ld_bias_req_rdy)
      .writes(ld_bias_req_fire, ld_bias_loop_id);
  UPDATE(updateAddress)
      .reads(loop0, loop1, ld_bias_req_fire, ld_bias_loop_id, ld_bias_addr_start_Q_)
      .writes(ld_bias_addr_start_D_);
  UPDATE(updateInputRequest).reads(loop0, loop1, head_loop_id)
      .writes(ld_input_req_val, ld_input_req_bits);
  UPDATE(updateInputTransfer).reads(ld_input_req_val, ld_input_req_bits, ld_input_req_rdy)
      .writes(ld_input_req_fire, ld_input_loop_id);
  UPDATE(updateWeightRequest).reads(loop0, loop1, head_loop_id)
      .writes(ld_weights_req_val, ld_weights_req_bits);
  UPDATE(updateWeightTransfer).reads(ld_weights_req_val, ld_weights_req_bits, ld_weights_req_rdy)
      .writes(ld_weights_req_fire, ld_weights_loop_id);
}

// Offer the head's weight job until started, then the tail's, independently of
// the other workers. Request-ready does not change selection or packet contents.
void LoopConvScheduler::updateWeightRequest() {
  const auto head      = static_cast<unsigned>(*head_loop_id);
  assert(head < 2);
  const auto head_slot = head == 0 ? *loop0 : *loop1;
  const auto selected  = head_slot.ld_weights_started == 1 ? head ^ 1u : head;
  const auto slot      = selected == 0 ? *loop0 : *loop1;
  LoopConvLdWeightReq request{};
  request.outer_bounds  = slot.outer_bounds;
  request.inner_bounds  = slot.inner_bounds;
  request.geometry.ochs = slot.inner_bounds.pochs;
  request.geometry.out_channels_per_bank = (std::uint32_t{slot.inner_bounds.pochs} + kDim - 1) / kDim;
  request.geometry.in_channels_per_bank  = (std::uint32_t{slot.inner_bounds.kchs} + kDim - 1) / kDim;
  const auto channels = slot.launch.trans_weight_0132 == 1 ? slot.inner_bounds.pochs : slot.inner_bounds.kchs;
  request.geometry.weight_spad_stride = static_cast<std::uint16_t>(std::uint64_t{slot.inner_bounds.krows} * slot.inner_bounds.kcols * channels);
  const auto spad_id  = slot.launch.b_ex_spad_id;
  const auto end      = spad_id == 0 ? slot.b_addr_end : spad_id * (kSpRows / 2);
  // Original uses log2Up(max_addr+1) bits: the full-memory endpoint is valid.
  request.addr_end          = static_cast<std::uint32_t>(end) & static_cast<std::uint32_t>(mask(log2Up(kSpRows + 1)));
  request.dram_addr         = slot.weights_dram_addr;
  request.trans_weight_1203 = slot.launch.trans_weight_1203;
  request.trans_weight_0132 = slot.launch.trans_weight_0132;
  request.dw                = slot.launch.dw;
  request.loop_id           = selected;
  ld_weights_req_bits       = request;
  ld_weights_req_val        = bit(slot.configured == 1 && slot.ld_weights_started == 0);
}

// Tell the state owner which weight job was accepted; it owns started flags.
void LoopConvScheduler::updateWeightTransfer() {
  ld_weights_req_fire = bit(ld_weights_req_val == 1 && ld_weights_req_rdy == 1);
  ld_weights_loop_id  = (*ld_weights_req_bits).loop_id;
  if (ld_weights_req_fire == 1)
    trace("loop_conv scheduler: weights accepted slot=%u", unsigned(*ld_weights_loop_id));
}

// Select the head's input job until started, then the tail's, independently of bias.
void LoopConvScheduler::updateInputRequest() {
  const auto head      = static_cast<unsigned>(*head_loop_id);
  assert(head < 2);
  const auto head_slot = head == 0 ? *loop0 : *loop1;
  const auto selected  = head_slot.ld_input_started == 1 ? head ^ 1u : head;
  const auto slot      = selected == 0 ? *loop0 : *loop1;
  LoopConvLdInputReq request{};
  request.outer_bounds = slot.outer_bounds;
  request.inner_bounds = slot.inner_bounds;
  request.geometry     = inputGeometry(slot);
  const auto spad_id   = slot.launch.a_ex_spad_id;
  const auto base      = spad_id == 0 ? slot.a_addr_start : (spad_id - 1u) * (kSpRows / 2);
  // Original assigns this value to a log2Up(max_addr)-bit request field.
  request.addr_start         = static_cast<std::uint32_t>(base) & kSpAddrMask;
  request.dram_addr          = slot.input_dram_addr;
  request.downsample         = slot.launch.downsample;
  request.max_pixels_per_row = slot.launch.max_pixels_per_row;
  request.input_dilated      = slot.launch.input_dilated;
  request.trans_input_3120   = slot.launch.trans_input_3120;
  request.loop_id            = selected;
  ld_input_req_bits          = request;
  ld_input_req_val           = bit(slot.configured == 1 && slot.ld_input_started == 0);
}

void LoopConvScheduler::updateInputTransfer() {
  ld_input_req_fire = bit(ld_input_req_val == 1 && ld_input_req_rdy == 1);
  ld_input_loop_id  = (*ld_input_req_bits).loop_id;
}

// Offer the head bank's bias job until accepted, then offer the tail bank's.
void LoopConvScheduler::updateRequest() {
  const auto head = static_cast<unsigned>(*head_loop_id);
  assert(head < 2);
  const auto head_slot = head == 0 ? *loop0 : *loop1;
  const auto selected  = head_slot.ld_bias_started == 1 ? head ^ 1u : head;
  const auto slot      = selected == 0 ? *loop0 : *loop1;
  LoopConvLdBiasReq request{};
  request.outer_bounds = slot.outer_bounds;
  request.inner_bounds = slot.inner_bounds;
  request.ochs = slot.inner_bounds.pochs;
  // Original stores this derived product in a 16-bit field.
  request.bias_spad_stride = static_cast<std::uint16_t>(std::uint64_t{slot.inner_bounds.batches} * slot.inner_bounds.orows * slot.inner_bounds.ocols);
  request.addr_start       = static_cast<std::uint32_t>(*ld_bias_addr_start_Q_);
  request.dram_addr        = slot.bias_dram_addr;
  request.no_bias          = slot.launch.no_bias;
  request.loop_id          = selected;
  ld_bias_req_bits         = request;
  ld_bias_req_val          = bit(slot.configured == 1 && slot.ld_bias_started == 0);
}

// Announce acceptance without making request-valid depend on worker-ready.
void LoopConvScheduler::updateTransfer() {
  ld_bias_req_fire = bit(ld_bias_req_val == 1 && ld_bias_req_rdy == 1);
  ld_bias_loop_id  = (*ld_bias_req_bits).loop_id;
}

// On acceptance, alternate accumulator halves when this loop has an output.
void LoopConvScheduler::updateAddress() {
  if (ld_bias_req_fire == 0) return;
  const auto id   = static_cast<unsigned>(*ld_bias_loop_id);
  const auto slot = id == 0 ? *loop0 : *loop1;
  if (slot.output_dram_addr != 0) {
    const auto current    = static_cast<std::uint32_t>(*ld_bias_addr_start_Q_);
    ld_bias_addr_start_D_ = (current + kAccRows / 2) % kAccRows;
  }
  trace("loop_conv scheduler: bias accepted slot=%u", id);
}

void LoopConvScheduler::reset() {
  ld_bias_addr_start_D_.reset(0);
  ld_bias_req_val.reset(0);
  ld_bias_req_bits.reset(LoopConvLdBiasReq{});
  ld_bias_req_fire.reset(0);
  ld_bias_loop_id.reset(0);
  ld_input_req_val.reset(0);
  ld_input_req_bits.reset(LoopConvLdInputReq{});
  ld_input_req_fire.reset(0);
  ld_input_loop_id.reset(0);
  ld_weights_req_val.reset(0);
  ld_weights_req_bits.reset(LoopConvLdWeightReq{});
  ld_weights_req_fire.reset(0);
  ld_weights_loop_id.reset(0);
}

} // namespace smesh
