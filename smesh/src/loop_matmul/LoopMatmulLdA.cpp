// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulLdA.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulLdA.hpp"

#include "SmeshCommand.hpp"

#include <algorithm>

namespace smesh {
namespace {

constexpr std::uint32_t kMaxBlocks =
    std::max<std::uint32_t>(1, kDefaultConfig.dma_max_bytes / (kDim * sizeof(Elem)));

std::uint16_t floorAdd(std::uint16_t value, std::uint16_t step, std::uint16_t limit) {
  return value + step >= limit ? 0 : value + step;
}

struct Position {
  std::uint16_t row;
  std::uint16_t col;
  std::uint16_t max_rows;
  std::uint16_t max_cols;
  std::uint16_t blocks;
};

Position position(const LoopMatmulLdAReq& req, std::uint16_t i, std::uint16_t k) {
  const bool transpose = req.transpose == 1;
  const auto row = transpose ? k : i;
  const auto col = transpose ? i : k;
  const auto max_rows = transpose ? req.max_k : req.max_i;
  const auto max_cols = transpose ? req.max_i : req.max_k;
  const auto max_blocks = std::min<std::uint16_t>(max_cols, kMaxBlocks);
  const auto blocks = static_cast<std::uint16_t>(
      col + max_blocks <= max_cols ? max_blocks : max_cols - col);
  return {row, col, max_rows, max_cols, blocks};
}

} // namespace

LoopMatmulLdA::LoopMatmulLdA(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, i, k, idle, loop_id);
  UPDATE(updateCommand)
      .reads(state_Q_, ld_utilization_at_limit)
      .writes(cmd_val, cmd_bits);
  UPDATE(updateNextState)
      .reads(state_Q_, req_val, req_rdy, req_bits, cmd_val, cmd_rdy)
      .writes(state_D_);
}

// reads current registered state (state_Q_) and drives req_rdy,
// idle, i, k, and loop_id outputs
void LoopMatmulLdA::updateStatus() {
  const auto state = *state_Q_;
  req_rdy = bit(state.active == 0);
  i = state.i;
  k = state.k;
  idle = bit(state.active == 0);
  loop_id = state.req.loop_id;
}

// builds MVIN cmd for current tile position (using same current state)
// asserts cmd_val only hen LdA is active, DRAM addr is non-zero, and utilization is below limit
void LoopMatmulLdA::updateCommand() {
  const auto state = *state_Q_;
  cmd_val = bit(state.active == 1 && ld_utilization_at_limit == 0 &&
                state.req.dram_addr != 0);
  cmd_bits = SmeshCmd{};
  if (state.active == 0) return;

  const auto& req = state.req;
  const auto pos = position(req, state.i, state.k);
  const auto row_pad = req.transpose == 1 ? req.pad_k : req.pad_i;
  const auto col_pad = req.transpose == 1 ? req.pad_i : req.pad_k;
  const auto rows = kDim - (pos.row == pos.max_rows - 1 ? row_pad : 0);
  const auto cols = pos.blocks * kDim -
                    (pos.col + pos.blocks >= pos.max_cols ? col_pad : 0);
  const auto offset = ((std::uint64_t{pos.row} * req.dram_stride + pos.col) *
                       kDim * sizeof(Elem)) & 0xffffffffull;
  const auto local_row = static_cast<std::uint32_t>(
      std::uint64_t{req.addr_start} +
      (std::uint64_t{pos.row} * pos.max_cols + pos.col) * kDim);
  const auto local_addr = req.is_resadd == 1
                              ? makeAccAddr(local_row)
                              : makeSpAddr(local_row);

  SmeshCmd command{};
  command.funct = static_cast<std::uint32_t>(SmeshFunct::Mvin);
  command.rs1 = req.dram_addr + offset;
  command.rs2 = packLocal(local_addr, {rows, cols});
  cmd_bits = command;
}

// decides which tile position comes next
// handles handshakes, captures new res when req_val && req_rdy, or
// advances i and k when cmd_val && cmd_rdy
// writes resulting state to state_D_ (which becomes state_Q_ on next cycle)
void LoopMatmulLdA::updateNextState() {
  const auto current = *state_Q_;
  if (current.active == 0) {
    if (req_val == 1 && req_rdy == 1) {
      auto next = current;
      next.req = *req_bits;
      next.i = 0;
      next.k = 0;
      next.active = 1;
      state_D_ = next;
      trace("ldA: accepted loop=%u I=%u K=%u",
            static_cast<unsigned>(next.req.loop_id), next.req.max_i, next.req.max_k);
    }
    return;
  }

  if (current.req.dram_addr == 0) {
    auto next = current;
    next.active = 0;
    state_D_ = next;
    return;
  }
  if (cmd_val == 0 || cmd_rdy == 0) return;

  auto next = current;
  const auto pos = position(current.req, current.i, current.k);
  const auto i_step = static_cast<std::uint16_t>(current.req.transpose == 1 ? pos.blocks : 1);
  const auto k_step = static_cast<std::uint16_t>(current.req.transpose == 1 ? 1 : pos.blocks);
  const auto next_i = floorAdd(current.i, i_step, current.req.max_i);
  const auto next_k = next_i == 0
                          ? floorAdd(current.k, k_step, current.req.max_k)
                          : static_cast<std::uint16_t>(current.k);
  next.i = next_i;
  next.k = next_k;
  next.active = bit(next_i != 0 || next_k != 0);
  state_D_ = next;
  trace("ldA: sent loop=%u i=%u k=%u next={%u,%u}",
        static_cast<unsigned>(current.req.loop_id),
        static_cast<unsigned>(current.i), static_cast<unsigned>(current.k),
        next_i, next_k);
}

void LoopMatmulLdA::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
  i.reset(0);
  k.reset(0);
  idle.reset(1);
  loop_id.reset(0);
}

} // namespace smesh
