// **********************************************************************
// smesh/src/loop_conv/LoopConvLdBias.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvLdBias.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cassert>

namespace smesh {
namespace {

constexpr std::uint32_t kMaxOchsPerMvin = kDim * std::max<std::size_t>(1, kDefaultConfig.dma_max_bytes / (kDim * sizeof(Acc)));

std::uint16_t floorAdd(std::uint16_t value, std::uint32_t step, std::uint16_t limit) {
  return std::uint32_t{value} + step >= limit ? 0 : value + step;
}

} // namespace

LoopConvLdBias::LoopConvLdBias(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, idle, loop_id);
  UPDATE(updateCommand).reads(state_Q_, ld_utilization_at_limit).writes(cmd_val, cmd_bits);
  UPDATE(updateNextState).reads(state_Q_, req_val, req_bits, req_rdy, cmd_rdy, ld_utilization_at_limit, wait_for_prev_loop)
      .writes(state_D_);
}

// A new job can start only after generation ends and both pipeline stages drain.
void LoopConvLdBias::updateStatus() {
  const auto state = *state_Q_;
  const bool empty = state.phase == Phase::Idle && state.valid[0] == 0 && state.valid[1] == 0;
  req_rdy          = bit(empty);
  idle             = bit(empty);
  loop_id          = state.req.loop_id;
}

// Present the pipeline's last command; RS pressure blocks its output handshake.
void LoopConvLdBias::updateCommand() {
  const auto state = *state_Q_;
  cmd_bits         = state.valid[1] == 1 ? state.commands[1] : SmeshCmd{};
  cmd_val          = bit(state.valid[1] == 1 && ld_utilization_at_limit == 0);
}

// Move registered pipeline stages and advance iterators only on pipeline input acceptance.
void LoopConvLdBias::updateNextState() {
  const auto current      = *state_Q_;
  auto       next         = current;
  const bool output_ready = cmd_rdy == 1 && ld_utilization_at_limit == 0;
  const bool stage1_ready = current.valid[1] == 0 || output_ready;
  const bool stage0_ready = current.valid[0] == 0 || stage1_ready;

  // Each stage takes the previous cycle's upstream value when it has room.
  if (stage1_ready) {
    next.valid[1]    = current.valid[0];
    next.commands[1] = current.valid[0] == 1 ? current.commands[0] : SmeshCmd{};
  }
  if (stage0_ready) {
    next.valid[0]    = 0;
    next.commands[0] = {};
  }

  const auto& req    = current.req;
  const bool skip    = req.dram_addr == 0;
  const bool enqueue = current.phase != Phase::Idle && !skip && wait_for_prev_loop == 0 && stage0_ready;
  if (skip) next.phase = Phase::Idle;
  if (enqueue) {
    SmeshCmd command{};
    if (current.phase == Phase::Config) {
      command.funct = static_cast<std::uint32_t>(SmeshFunct::Config);
      command.rs1   = packConfigLoadRs1(2, req.bias_spad_stride, kMvinScaleIdentityBits, false, 1);
      command.rs2   = 0;
      next.phase    = Phase::Load;
    } else {
      const auto  step      = std::min<std::uint32_t>(req.ochs, kMaxOchsPerMvin);
      const auto  rows      = std::min<std::uint32_t>(req.inner_bounds.ocols - current.ocol, kDim);
      const auto  cols      = std::min<std::uint32_t>(req.ochs - current.och, step);
      const auto& bounds    = req.inner_bounds;
      const auto  local_row = static_cast<std::uint32_t>(
          std::uint64_t{req.addr_start} +
          (current.och / kDim) * std::uint64_t{bounds.batches} * bounds.orows * bounds.ocols +
          std::uint64_t{current.b} * bounds.orows * bounds.ocols +
          std::uint64_t{current.orow} * bounds.ocols + current.ocol);
      const auto offset     = (std::uint64_t{current.och} * sizeof(Acc)) & 0xffffffffull;
      command.funct         = static_cast<std::uint32_t>(SmeshFunct::Mvin3);
      command.rs1           = req.no_bias == 1 ? 0 : req.dram_addr + offset;
      command.rs2           = packLocal(makeAccAddr(local_row, false, false), {rows, cols});

      next.och  = floorAdd(current.och, step, req.ochs);
      next.ocol = next.och == 0 ? floorAdd(current.ocol, kDim, bounds.ocols) : current.ocol;
      next.orow = next.och == 0 && next.ocol == 0 ? floorAdd(current.orow, 1, bounds.orows) : current.orow;
      next.b    = next.och == 0 && next.ocol == 0 && next.orow == 0 ? floorAdd(current.b, 1, bounds.batches) : current.b;
      if (next.och == 0 && next.ocol == 0 && next.orow == 0 && next.b == 0)
        next.phase = Phase::Idle;
    }
    // The final command is encoded here, rather than after the pipeline as in Original.
    // Its request and iterator values are captured at the same input handshake.
    next.commands[0] = command;
    next.valid[0]    = 1;
    trace("loop_conv bias: queued funct=%u loop=%u b=%u orow=%u ocol=%u och=%u",
          static_cast<unsigned>(command.funct), unsigned(req.loop_id),
          current.b, current.orow, current.ocol, current.och);
  }

  // Request acceptance wins over the previous job's skip/idle transition.
  if (req_val == 1 && req_rdy == 1) {
    next.req = *req_bits;
    if (next.req.dram_addr != 0) {
      assert(next.req.ochs > 0 && next.req.inner_bounds.ocols > 0 && next.req.inner_bounds.orows > 0 && next.req.inner_bounds.batches > 0);
    }
    next.phase = Phase::Config;
    next.b     = next.orow = next.ocol = next.och = 0;
    trace("loop_conv bias: accepted loop=%u", unsigned(next.req.loop_id));
  }
  if (current.valid[1] == 1 && output_ready)
    trace("loop_conv bias: sent funct=%u loop=%u", static_cast<unsigned>(current.commands[1].funct), unsigned(req.loop_id));
  state_D_ = next;
}

void LoopConvLdBias::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  idle.reset(1);
  loop_id.reset(0);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
}

} // namespace smesh
