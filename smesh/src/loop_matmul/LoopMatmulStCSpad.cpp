#include "LoopMatmulStCSpad.hpp"

#include "SmeshCommand.hpp"

#include <cstdint>

namespace smesh {
namespace {

std::uint16_t floorAdd(std::uint16_t value, std::uint16_t limit) {
  return value + 1 >= limit ? 0 : value + 1;
}

bool isNormalization(std::uint8_t act) {
  return act == 2 || act == 4; // LAYERNORM and SOFTMAX
}

} // namespace

LoopMatmulStCSpad::LoopMatmulStCSpad(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, j, i, idle, loop_id);
  UPDATE(updateCommand)
      .reads(state_Q_, ex_k, ex_j, ex_i, ex_completed, st_utilization_at_limit)
      .writes(cmd_val, cmd_bits);
  UPDATE(updateNextState)
      .reads(state_Q_, req_val, req_rdy, req_bits, cmd_val, cmd_rdy)
      .writes(state_D_);
}

void LoopMatmulStCSpad::updateStatus() {
  const auto state = *state_Q_;
  req_rdy = bit(state.active == 0);
  j = state.j;
  i = state.i;
  idle = bit(state.active == 0);
  loop_id = state.req.loop_id;
}

void LoopMatmulStCSpad::updateCommand() {
  const auto state = *state_Q_;
  const auto& req = state.req;
  const auto current_i = static_cast<std::uint16_t>(state.i);
  const auto current_j = static_cast<std::uint16_t>(state.j);
  // Gemmini's StCSpad fixes max_block_len to one.
  const auto blocks = static_cast<std::uint16_t>(req.max_j > current_j ? 1 : 0);
  const auto ex_k_value = static_cast<std::uint16_t>(*ex_k);
  const auto ex_j_value = static_cast<std::uint16_t>(*ex_j);
  const auto ex_i_value = static_cast<std::uint16_t>(*ex_i);
  bool ex_ahead = ex_completed == 1 ||
                  (!isNormalization(req.act) && ex_k_value == req.max_k - 1 &&
                   (ex_j_value >= current_j + blocks ||
                    (ex_j_value == current_j + blocks - 1 && ex_i_value > current_i)));
  if (req.is_resadd == 1) {
    ex_ahead = ex_completed == 1 || ex_i_value > current_i ||
               (ex_i_value == current_i && ex_j_value >= current_j + blocks);
  }
  cmd_val = bit(state.active == 1 && st_utilization_at_limit == 0 && ex_ahead);

  cmd_bits = SmeshCmd{};
  if (state.active == 0) return;

  const auto offset = static_cast<std::uint32_t>(
      (std::uint64_t{current_i} * req.max_j + current_j) * kDim);
  const auto cols = static_cast<std::uint16_t>(kDim -
      (current_j + blocks >= req.max_j ? req.pad_j : 0));
  const auto rows = static_cast<std::uint16_t>(kDim -
      (current_i == req.max_i - 1 ? req.pad_i : 0));
  SmeshCmd command{};
  command.funct = static_cast<std::uint32_t>(SmeshFunct::StoreSpad);
  command.rs1 = packStoreSpadDestination(makeSpAddr(req.dst_addr + offset), 1);
  command.rs2 = packLocal(makeAccAddr(req.src_addr + offset, false, req.full_c == 1),
                          {rows, cols});
  cmd_bits = command;
}

void LoopMatmulStCSpad::updateNextState() {
  const auto current = *state_Q_;
  if (current.active == 0) {
    if (req_val == 1 && req_rdy == 1) {
      auto next = current;
      next.req = *req_bits;
      next.i = 0;
      next.j = 0;
      next.active = 1;
      state_D_ = next;
      trace("lmStCSpad: accepted loop=%u I=%u J=%u", next.req.loop_id,
            next.req.max_i, next.req.max_j);
    }
    return;
  }
  if (cmd_val == 0 || cmd_rdy == 0) return;

  auto next = current;
  next.i = floorAdd(current.i, current.req.max_i);
  if (next.i == 0) next.j = floorAdd(current.j, current.req.max_j);
  if (next.i == 0 && next.j == 0) next.active = 0;
  state_D_ = next;
  trace("lmStCSpad: sent loop=%u ij={%u,%u}", current.req.loop_id,
        static_cast<unsigned>(current.i), static_cast<unsigned>(current.j));
}

void LoopMatmulStCSpad::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
  j.reset(0);
  i.reset(0);
  idle.reset(1);
  loop_id.reset(0);
}

} // namespace smesh
