// **********************************************************************
// smesh/src/loop_conv/LoopConvExecute.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvExecute.hpp"
#include "SmeshCommand.hpp"
#include <algorithm>
#include <cassert>

namespace smesh {
namespace {

// Original's floorAdd wraps at the command bound, not the 16-bit register width.
std::uint16_t floorAdd(std::uint16_t value, std::uint32_t step, std::uint16_t limit, bool enable = true) {
  if (!enable) return value;
  return std::uint32_t{value} + step >= limit ? 0 : static_cast<std::uint16_t>(value + step);
}

std::uint64_t undilated(std::uint64_t value, bool dilated) {
  return (value + unsigned(dilated)) >> unsigned(dilated);
}

} // namespace

LoopConvExecute::LoopConvExecute(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, idle, loop_id);
  UPDATE(updateCommand).reads(state_Q_, ex_utilization_at_limit).writes(cmd_val, cmd_bits);
  UPDATE(updateNextState).reads(state_Q_, req_val, req_bits, req_rdy, cmd_rdy)
      .reads(lda_completed, ldb_completed, ldd_completed, ex_utilization_at_limit)
      .writes(state_D_);
}

// A job finishes only when generation has ended and both command stages drain.
void LoopConvExecute::updateStatus() {
  const auto q     = *state_Q_;
  const bool empty = q.phase == Phase::Idle && q.valid[0] == 0 && q.valid[1] == 0;
  idle             = bit(empty);
  req_rdy          = bit(empty);
  loop_id          = q.req.loop_id;
}

// RS pressure hides valid without removing the saved command from the pipeline.
void LoopConvExecute::updateCommand() {
  const auto q = *state_Q_;
  cmd_val      = bit(q.valid[1] == 1 && ex_utilization_at_limit == 0);
  cmd_bits     = q.valid[1] == 1 ? q.commands[1] : SmeshCmd{};
}

// Input dilation inserts zeros between source positions. Original skips the
// odd coordinates; subtraction underflow does not change the low parity bit.
bool LoopConvExecute::skipIteration(const State& q) {
  if (q.phase < Phase::Preload || q.req.input_dilated == 0) return false;
  const auto& outer = q.req.outer_bounds;
  const auto& inner = q.req.inner_bounds;
  const auto  row   = std::uint64_t{q.krow} * outer.kernel_dilation + q.orow - inner.upad;
  const auto  col   = std::uint64_t{q.kcol} * outer.kernel_dilation + q.ocol - inner.lpad;
  return (row & 1u) != 0 || (col & 1u) != 0;
}

// Build a complete command from the saved job and this cycle's coordinates.
SmeshCmd LoopConvExecute::makeCommand(const State& q) {
  const auto& req    = q.req;
  const auto& inner  = req.inner_bounds;
  const auto& outer  = req.outer_bounds;
  const auto& input  = req.input_geometry;
  const auto& weight = req.weight_geometry;
  SmeshCmd cmd{};
  if (q.phase == Phase::Config) {
    cmd.funct = static_cast<std::uint32_t>(SmeshFunct::Config);
    // Only strides are meaningful here; zero-fill Original's DontCare fields.
    cmd.rs1 = packConfigExRs1(std::uint32_t{input.irows} * input.icols, false, false, 0, true, 0, 0);
    cmd.rs2 = packConfigExRs2(std::uint32_t{inner.orows} * inner.ocols);
    return cmd;
  }
  const bool dilated        = req.input_dilated == 1;
  const bool trans_input    = req.trans_input_3120 == 1;
  const bool trans_weight   = req.trans_weight_0132 == 1;
  const unsigned downsample = req.downsample == 1 ? 1 : 0;
  const auto pixels = std::min<std::uint32_t>(inner.kcols - q.kcol, req.max_pixels_per_row);
  const auto i = trans_input ? std::min<std::uint64_t>(inner.batches - q.b, kDim) : undilated(std::min<std::uint64_t>(inner.ocols - q.ocol, kDim << unsigned(dilated)), dilated);
  const auto j = std::min<std::uint32_t>(weight.ochs - q.och, kDim);
  const auto k = pixels * std::min<std::uint32_t>(inner.kchs - q.kch, kDim);
  const auto irow = undilated(std::uint64_t{q.orow} * outer.stride + std::uint64_t{q.krow} * outer.kernel_dilation, dilated);
  const auto icol = undilated(std::uint64_t{q.ocol} * outer.stride + std::uint64_t{q.kcol} * outer.kernel_dilation, dilated);
  const auto rows = input.irows >> downsample;
  const auto cols = input.icols >> downsample;
  const auto a = std::uint64_t{req.a_addr_start} +
      (trans_input ? q.b / kDim : q.kch / kDim) * std::uint64_t{input.input_spad_stride} +
      std::uint64_t{trans_input ? q.kch : q.b} * rows * cols +
      (irow >> downsample) * cols + (icol >> downsample);
  const auto c = std::uint64_t{req.c_addr_start} +
      (q.och / kDim) * std::uint64_t{inner.batches} * inner.orows * inner.ocols +
      std::uint64_t{q.b} * inner.orows * inner.ocols + std::uint64_t{q.orow} * inner.ocols + q.ocol;
  const auto weight_rows = std::uint64_t{trans_weight ? weight.in_channels_per_bank : weight.out_channels_per_bank} *
      inner.kcols * inner.krows * (trans_weight ? weight.ochs : inner.kchs);
  const auto rotated_row = req.wrot180 == 1 ? inner.krows - q.krow - 1 : q.krow;
  const auto rotated_col = req.wrot180 == 1 ? inner.kcols - q.kcol - 1 : q.kcol;
  const auto channels    = trans_weight ? weight.ochs : inner.kchs;
  const auto b = std::uint64_t{req.b_addr_end} - weight_rows +
      (trans_weight ? q.kch / kDim : q.och / kDim) * std::uint64_t{inner.krows} * inner.kcols * channels +
      std::uint64_t{static_cast<unsigned>(rotated_row)} * inner.kcols * channels +
      std::uint64_t{static_cast<unsigned>(rotated_col)} * channels + (trans_weight ? q.och : q.kch);
  constexpr std::uint32_t garbage = 0xffffffffu;
  if (q.phase == Phase::Preload) {
    cmd.funct = static_cast<std::uint32_t>(SmeshFunct::Preload);
    cmd.rs1 = packLocal(q.new_weights == 1 ? makeSpAddr(static_cast<std::uint32_t>(b)).raw : garbage, {k, j});
    cmd.rs2 = packLocal(makeAccAddr(static_cast<std::uint32_t>(c), true, false), {i, j});
  } else {
    cmd.funct = static_cast<std::uint32_t>(q.new_weights == 1 ? SmeshFunct::ComputeFlip : SmeshFunct::ComputeStay);
    cmd.rs1 = packLocal(makeSpAddr(static_cast<std::uint32_t>(a)), {i, k});
    cmd.rs2 = packLocal(garbage, {i, j});
  }
  return cmd;
}

// Advance the elastic pipeline and FSM from current Q only. Skipped positions
// advance without enqueueing; ordinary positions advance only on enqueue.
void LoopConvExecute::updateNextState() {
  const auto q            = *state_Q_;
  auto next               = q;
  const bool output_ready = cmd_rdy == 1 && ex_utilization_at_limit == 0;
  const bool stage1_ready = q.valid[1] == 0 || output_ready;
  const bool stage0_ready = q.valid[0] == 0 || stage1_ready;
  if (stage1_ready) {
    next.valid[1] = q.valid[0];
    next.commands[1] = q.valid[0] == 1 ? q.commands[0] : SmeshCmd{};
  }
  if (stage0_ready) {
    next.valid[0]    = 0;
    next.commands[0] = {};
  }
  const bool skip        = skipIteration(q);
  const bool loads_ready = lda_completed == 1 && ldb_completed == 1 && ldd_completed == 1;
  const bool enqueue     = q.phase != Phase::Idle && !skip && loads_ready && stage0_ready;
  if (enqueue) {
    next.commands[0] = makeCommand(q);
    next.valid[0]    = 1;
    trace("loop_conv execute: queued funct=%u loop=%u och=%u kr=%u kc=%u ch=%u b=%u row=%u col=%u",
          unsigned(next.commands[0].funct), unsigned(q.req.loop_id), q.och, q.krow, q.kcol, q.kch, q.b, q.orow, q.ocol);
  }
  if (q.phase == Phase::Compute && enqueue) next.new_weights = 0;
  if (enqueue || skip) {
    if (q.phase == Phase::Config) next.phase = Phase::Preload;
    else if (q.phase == Phase::Preload) next.phase = Phase::Compute;
    else {
      const auto& inner       = q.req.inner_bounds;
      const auto ocol_step    = skip || q.req.trans_input_3120 == 1 ? 1 : kDim << unsigned(q.req.input_dilated == 1);
      next.ocol               = floorAdd(q.ocol, ocol_step, inner.ocols);
      next.orow               = floorAdd(q.orow, 1, inner.orows, next.ocol == 0);
      const bool spatial_wrap = next.orow == 0 && next.ocol == 0;
      next.b                  = floorAdd(q.b, q.req.trans_input_3120 == 1 ? kDim : 1, inner.batches, spatial_wrap);
      const bool batch_wrap   = spatial_wrap && next.b == 0;
      next.kch                = floorAdd(q.kch, kDim, inner.kchs, batch_wrap);
      next.kcol               = floorAdd(q.kcol, q.req.max_pixels_per_row, inner.kcols, batch_wrap && next.kch == 0);
      next.krow               = floorAdd(q.krow, 1, inner.krows, batch_wrap && next.kch == 0 && next.kcol == 0);
      next.och                = floorAdd(q.och, kDim, q.req.weight_geometry.ochs, batch_wrap && next.kch == 0 && next.kcol == 0 && next.krow == 0);
      if (batch_wrap) next.new_weights = 1;
      next.phase = batch_wrap && next.kch == 0 && next.kcol == 0 && next.krow == 0 && next.och == 0
          ? Phase::Idle : Phase::Preload;
    }
    if (skip) trace("loop_conv execute: skipped loop=%u row=%u col=%u", unsigned(q.req.loop_id), q.orow, q.ocol);
  }
  if (req_val == 1 && req_rdy == 1) {
    next.req          = *req_bits;
    const auto& inner = next.req.inner_bounds;
    assert(inner.batches > 0 && inner.orows > 0 && inner.ocols > 0 && inner.kchs > 0 &&
           inner.krows > 0 && inner.kcols > 0 && next.req.weight_geometry.ochs > 0 && next.req.max_pixels_per_row > 0);
    next.phase       = next.req.trans_input_3120 == 1 ? Phase::Config : Phase::Preload;
    next.och         = next.krow = next.kcol = next.kch = next.b = next.orow = next.ocol = 0;
    next.new_weights = 1;
    trace("loop_conv execute: accepted loop=%u", unsigned(next.req.loop_id));
  }
  if (q.valid[1] == 1 && output_ready)
    trace("loop_conv execute: sent funct=%u loop=%u", unsigned(q.commands[1].funct), unsigned(q.req.loop_id));
  state_D_ = next;
}

void LoopConvExecute::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  idle.reset(1);
  loop_id.reset(0);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
}

} // namespace smesh
