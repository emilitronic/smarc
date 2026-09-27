// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulLdB.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulLdB.hpp"

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

Position position(const LoopMatmulLdBReq& req, std::uint16_t k, std::uint16_t j) {
  const bool transpose = req.transpose == 1;
  const auto row = transpose ? j : k;
  const auto col = transpose ? k : j;
  const auto max_rows = transpose ? req.max_j : req.max_k;
  const auto max_cols = transpose ? req.max_k : req.max_j;
  const auto max_blocks = std::min<std::uint16_t>(max_cols, kMaxBlocks);
  const auto blocks = static_cast<std::uint16_t>(
      col + max_blocks <= max_cols ? max_blocks : max_cols - col);
  return {row, col, max_rows, max_cols, blocks};
}

} // namespace

LoopMatmulLdB::LoopMatmulLdB(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, k, j, idle, loop_id);
  UPDATE(updateCommand)
      .reads(state_Q_, ld_utilization_at_limit)
      .writes(cmd_val, cmd_bits);
  UPDATE(updateNextState)
      .reads(state_Q_, req_val, req_rdy, req_bits, cmd_val, cmd_rdy)
      .writes(state_D_);
}

void LoopMatmulLdB::updateStatus() {
  const auto state = *state_Q_;
  req_rdy = bit(state.active == 0);
  k = state.k;
  j = state.j;
  idle = bit(state.active == 0);
  loop_id = state.req.loop_id;
}

void LoopMatmulLdB::updateCommand() {
  const auto state = *state_Q_;
  cmd_val = bit(state.active == 1 && ld_utilization_at_limit == 0 &&
                state.req.dram_addr != 0);
  cmd_bits = SmeshCmd{};
  if (state.active == 0) return;

  const auto& req = state.req;
  const auto pos = position(req, state.k, state.j);
  const auto col_pad = req.transpose == 1 ? req.pad_k : req.pad_j;
  // Gemmini's LdB compares max_row_iterator with itself minus one, so row_pad is never applied.
  const auto rows = kDim;
  const auto cols = pos.blocks * kDim -
                    (pos.col + pos.blocks >= pos.max_cols ? col_pad : 0);
  const auto offset = ((std::uint64_t{pos.row} * req.dram_stride + pos.col) *
                       kDim * sizeof(Elem)) & 0xffffffffull;
  const auto region_rows = std::uint64_t{req.max_k} * req.max_j * kDim;
  const auto local_start = req.is_resadd == 1
                               ? std::uint64_t{req.addr_end}
                               : std::uint64_t{req.addr_end} - region_rows;
  const auto local_row = static_cast<std::uint32_t>(
      local_start + (std::uint64_t{pos.row} * pos.max_cols + pos.col) * kDim);
  const auto local_addr = req.is_resadd == 1
                              ? makeAccAddr(local_row, true)
                              : makeSpAddr(local_row);

  SmeshCmd command{};
  command.funct = static_cast<std::uint32_t>(SmeshFunct::Mvin2);
  command.rs1 = req.dram_addr + offset;
  command.rs2 = packLocal(local_addr, {rows, cols});
  cmd_bits = command;
}

void LoopMatmulLdB::updateNextState() {
  const auto current = *state_Q_;
  if (current.active == 0) {
    if (req_val == 1 && req_rdy == 1) {
      auto next = current;
      next.req = *req_bits;
      next.k = 0;
      next.j = 0;
      next.active = 1;
      state_D_ = next;
      trace("ldB: accepted loop=%u K=%u J=%u",
            static_cast<unsigned>(next.req.loop_id), next.req.max_k, next.req.max_j);
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
  const auto pos = position(current.req, current.k, current.j);
  const auto j_step = static_cast<std::uint16_t>(current.req.transpose == 1 ? 1 : pos.blocks);
  const auto k_step = static_cast<std::uint16_t>(current.req.transpose == 1 ? pos.blocks : 1);
  const auto next_j = floorAdd(current.j, j_step, current.req.max_j);
  const auto next_k = next_j == 0
                          ? floorAdd(current.k, k_step, current.req.max_k)
                          : static_cast<std::uint16_t>(current.k);
  next.j = next_j;
  next.k = next_k;
  next.active = bit(next_j != 0 || next_k != 0);
  state_D_ = next;
  trace("ldB: sent loop=%u k=%u j=%u next={%u,%u}",
        static_cast<unsigned>(current.req.loop_id),
        static_cast<unsigned>(current.k), static_cast<unsigned>(current.j),
        next_k, next_j);
}

void LoopMatmulLdB::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
  k.reset(0);
  j.reset(0);
  idle.reset(1);
  loop_id.reset(0);
}

} // namespace smesh
