// **********************************************************************
// smesh/src/loop_conv/LoopConvScheduler.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvScheduler.hpp"
#include "SmeshTypes.hpp"

#include <cassert>

namespace smesh {

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
}

} // namespace smesh
