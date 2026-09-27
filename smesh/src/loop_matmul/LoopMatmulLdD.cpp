// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulLdD.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulLdD.hpp"

#include "SmeshCommand.hpp"

#include <algorithm>

namespace smesh {
namespace {

constexpr std::uint32_t kMaxInputBlocks =
    std::max<std::uint32_t>(1, kDefaultConfig.dma_max_bytes / (kDim * sizeof(Elem)));
constexpr std::uint32_t kMaxAccBlocks =
    std::max<std::uint32_t>(1, kDefaultConfig.dma_max_bytes / (kDim * sizeof(Acc)));

std::uint16_t floorAdd(std::uint16_t value, std::uint16_t step, std::uint16_t limit) {
  return value + step >= limit ? 0 : value + step;
}

std::uint16_t maxBlocks(const LoopMatmulLdDReq& req) {
  const auto limit = req.low_d == 1 ? kMaxInputBlocks : kMaxAccBlocks;
  return static_cast<std::uint16_t>(std::min<std::uint32_t>(req.max_j, limit));
}

} // namespace

LoopMatmulLdD::LoopMatmulLdD(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, idle, loop_id);
  UPDATE(updateCommand)
      .reads(state_Q_, ld_utilization_at_limit)
      .writes(cmd_val, cmd_bits);
  UPDATE(updateNextState)
      .reads(state_Q_, req_val, req_rdy, req_bits, cmd_val, cmd_rdy)
      .writes(state_D_);
}

void LoopMatmulLdD::updateStatus() {
  const auto state = *state_Q_;
  req_rdy = bit(state.active == 0);
  idle = bit(state.active == 0);
  loop_id = state.req.loop_id;
}

void LoopMatmulLdD::updateCommand() {
  const auto state = *state_Q_;
  cmd_val = bit(state.active == 1 && ld_utilization_at_limit == 0 &&
                state.req.dram_addr != 0);
  cmd_bits = SmeshCmd{};
  if (state.active == 0) return;

  const auto& req = state.req;
  const auto max_blocks = maxBlocks(req);
  const auto blocks = static_cast<std::uint16_t>(
      state.j + max_blocks <= req.max_j ? max_blocks : req.max_j - state.j);
  const auto rows = kDim - (state.i == req.max_i - 1 ? req.pad_i : 0);
  const auto cols = blocks * kDim -
                    (state.j + blocks >= req.max_j ? req.pad_j : 0);
  const auto element_bytes = req.low_d == 1 ? sizeof(Elem) : sizeof(Acc);
  const auto offset = ((std::uint64_t{state.i} * req.dram_stride + state.j) *
                       kDim * element_bytes) & 0xffffffffull;
  const auto local_row = static_cast<std::uint32_t>(
      std::uint64_t{req.addr_start} +
      (std::uint64_t{state.i} * req.max_j + state.j) * kDim);

  SmeshCmd command{};
  command.funct = static_cast<std::uint32_t>(SmeshFunct::Mvin3);
  command.rs1 = req.dram_addr + offset;
  command.rs2 = packLocal(makeAccAddr(local_row), {rows, cols});
  cmd_bits = command;
}

void LoopMatmulLdD::updateNextState() {
  const auto current = *state_Q_;
  if (current.active == 0) {
    if (req_val == 1 && req_rdy == 1) {
      auto next = current;
      next.req = *req_bits;
      next.i = 0;
      next.j = 0;
      next.active = 1;
      state_D_ = next;
      trace("ldD: accepted loop=%u I=%u J=%u",
            static_cast<unsigned>(next.req.loop_id), next.req.max_i, next.req.max_j);
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
  const auto next_i = floorAdd(current.i, 1, current.req.max_i);
  const auto next_j = next_i == 0
                          ? floorAdd(current.j, maxBlocks(current.req), current.req.max_j)
                          : static_cast<std::uint16_t>(current.j);
  next.i = next_i;
  next.j = next_j;
  next.active = bit(next_i != 0 || next_j != 0);
  state_D_ = next;
  trace("ldD: sent loop=%u i=%u j=%u next={%u,%u}",
        static_cast<unsigned>(current.req.loop_id),
        static_cast<unsigned>(current.i), static_cast<unsigned>(current.j),
        next_i, next_j);
}

void LoopMatmulLdD::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
  idle.reset(1);
  loop_id.reset(0);
}

} // namespace smesh
