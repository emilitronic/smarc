// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulEx.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulEx.hpp"

#include "SmeshCommand.hpp"

#include <cassert>

namespace smesh {
namespace {

std::uint16_t floorAdd(std::uint16_t value, std::uint16_t step, std::uint16_t limit) {
  return value + step >= limit ? 0 : value + step;
}

} // namespace

LoopMatmulEx::LoopMatmulEx(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, k, j, i, idle, loop_id);
  UPDATE(updateCommand)
      .reads(state_Q_, ld_ka, ld_kb, ld_j, ld_i,
             lda_completed, ldb_completed, ldd_completed)
      .reads(ex_utilization_at_limit)
      .writes(cmd_val, cmd_bits);
  UPDATE(updateNextState)
      .reads(state_Q_, req_val, req_rdy, req_bits, cmd_val, cmd_rdy)
      .writes(state_D_);
}

void LoopMatmulEx::updateStatus() {
  const auto state = *state_Q_;
  req_rdy = bit(state.phase == Phase::Idle);
  k = state.k;
  j = state.j;
  i = state.i;
  idle = bit(state.phase == Phase::Idle);
  loop_id = state.req.loop_id;
}

void LoopMatmulEx::updateCommand() {
  const auto state = *state_Q_;
  const auto& req = state.req;
  const auto current_k = static_cast<std::uint16_t>(state.k);
  const auto current_j = static_cast<std::uint16_t>(state.j);
  const auto current_i = static_cast<std::uint16_t>(state.i);
  const auto ka = static_cast<std::uint16_t>(*ld_ka);
  const auto kb = static_cast<std::uint16_t>(*ld_kb);

  const bool a_ahead = lda_completed == 1 || ka > current_k ||
                       (ka == current_k && ld_i > current_i);
  // Preserve Gemmini's ld_ka comparison in this B-progress condition.
  const bool b_ahead = ldb_completed == 1 || kb > current_k ||
                       (ka == current_k && ld_j > current_j);
  const bool d_ahead = ldd_completed == 1;
  cmd_val = bit(state.phase != Phase::Idle && ex_utilization_at_limit == 0 &&
                a_ahead && b_ahead && d_ahead && req.skip == 0);
  cmd_bits = SmeshCmd{};
  if (state.phase == Phase::Idle) return;

  assert(!(req.a_transpose == 1 && req.b_transpose == 1));
  const auto a_row = req.a_transpose == 1 ? current_k : current_i;
  const auto a_col = req.a_transpose == 1 ? current_i : current_k;
  const auto b_row = req.b_transpose == 1 ? current_j : current_k;
  const auto b_col = req.b_transpose == 1 ? current_k : current_j;
  const auto a_max_col = req.a_transpose == 1 ? req.max_i : req.max_k;
  const auto b_max_col = req.b_transpose == 1 ? req.max_k : req.max_j;
  const auto b_start = std::uint64_t{req.b_addr_end} -
                       std::uint64_t{req.max_k} * req.max_j * kDim;
  const auto a_addr = static_cast<std::uint32_t>(
      std::uint64_t{req.a_addr_start} +
      (std::uint64_t{a_row} * a_max_col + a_col) * kDim);
  const auto b_addr = static_cast<std::uint32_t>(
      b_start + (std::uint64_t{b_row} * b_max_col + b_col) * kDim);
  const auto c_addr = static_cast<std::uint32_t>(
      std::uint64_t{req.c_addr_start} +
      (std::uint64_t{current_i} * req.max_j + current_j) * kDim);

  const auto a_cols = kDim - (current_k == req.max_k - 1 ? req.pad_k : 0);
  const auto a_rows = kDim - (current_i == req.max_i - 1 ? req.pad_i : 0);
  const auto b_cols = kDim - (current_j == req.max_j - 1 ? req.pad_j : 0);
  const auto b_rows = kDim - (current_k == req.max_k - 1 ? req.pad_k : 0);
  const auto c_cols = kDim - (current_j == req.max_j - 1 ? req.pad_j : 0);
  const auto c_rows = kDim - (current_i == req.max_i - 1 ? req.pad_i : 0);

  SmeshCmd command{};
  if (state.phase == Phase::Preload) {
    command.funct = static_cast<std::uint32_t>(SmeshFunct::Preload);
    const auto b_operand = current_i == 0
                               ? makeSpAddr(b_addr)
                               : makeLocalAddr(0xffffffffu);
    command.rs1 = packLocal(b_operand, {b_rows, b_cols});
    command.rs2 = packLocal(makeAccAddr(c_addr, req.accumulate == 1 || current_k != 0),
                            {c_rows, c_cols});
  } else {
    command.funct = static_cast<std::uint32_t>(
        current_i == 0 ? SmeshFunct::ComputeFlip : SmeshFunct::ComputeStay);
    command.rs1 = packLocal(makeSpAddr(a_addr), {a_rows, a_cols});
    command.rs2 = packLocal(makeLocalAddr(0xffffffffu), {kDim, kDim});
  }
  cmd_bits = command;
}

void LoopMatmulEx::updateNextState() {
  const auto current = *state_Q_;
  if (current.phase == Phase::Idle) {
    if (req_val == 1 && req_rdy == 1) {
      auto next = current;
      next.req = *req_bits;
      next.k = 0;
      next.j = 0;
      next.i = 0;
      next.phase = Phase::Preload;
      state_D_ = next;
      trace("lmEx: accepted loop=%u K=%u J=%u I=%u",
            static_cast<unsigned>(next.req.loop_id), next.req.max_k,
            next.req.max_j, next.req.max_i);
    }
    return;
  }

  if (current.req.skip == 1) {
    auto next = current;
    next.phase = Phase::Idle;
    state_D_ = next;
    return;
  }
  if (cmd_val == 0 || cmd_rdy == 0) return;

  auto next = current;
  if (current.phase == Phase::Preload) {
    next.phase = Phase::Compute;
  } else {
    const auto next_i = floorAdd(current.i, 1, current.req.max_i);
    const auto next_j = next_i == 0
                            ? floorAdd(current.j, 1, current.req.max_j)
                            : static_cast<std::uint16_t>(current.j);
    const auto next_k = next_i == 0 && next_j == 0
                            ? floorAdd(current.k, 1, current.req.max_k)
                            : static_cast<std::uint16_t>(current.k);
    next.i = next_i;
    next.j = next_j;
    next.k = next_k;
    next.phase = next_i == 0 && next_j == 0 && next_k == 0
                     ? Phase::Idle : Phase::Preload;
  }
  state_D_ = next;
  trace("lmEx: sent loop=%u phase=%u kji={%u,%u,%u}",
        static_cast<unsigned>(current.req.loop_id),
        static_cast<unsigned>(current.phase),
        static_cast<unsigned>(current.k), static_cast<unsigned>(current.j),
        static_cast<unsigned>(current.i));
}

void LoopMatmulEx::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
  k.reset(0);
  j.reset(0);
  i.reset(0);
  idle.reset(1);
  loop_id.reset(0);
}

} // namespace smesh
