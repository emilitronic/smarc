// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulStC.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulStC.hpp"

#include "SmeshCommand.hpp"

#include <algorithm>
#include <cstdint>

namespace smesh {
namespace {

constexpr std::uint32_t kMaxBlocks =
    std::max<std::uint32_t>(1, kDefaultConfig.dma_max_bytes / (kDim * sizeof(Elem)));
constexpr std::uint16_t kNormStatIds = 2;
constexpr std::uint16_t kNormCmds = 3;
constexpr std::uint8_t kLayerNorm = 2;
constexpr std::uint8_t kSoftmax = 4;
constexpr std::uint32_t kConfigNorm = 3;

std::uint16_t floorAdd(std::uint16_t value, std::uint16_t step, std::uint16_t limit) {
  return value + step >= limit ? 0 : value + step;
}

bool isNormalization(std::uint8_t act) {
  return act == kLayerNorm || act == kSoftmax;
}

std::uint16_t maxBlocks(const LoopMatmulStCReq& req) {
  return req.full_c == 1 ? 1 :
      static_cast<std::uint16_t>(std::min<std::uint32_t>(req.max_j, kMaxBlocks));
}

std::uint32_t normCmd(std::uint8_t act, std::uint16_t phase, bool last_block) {
  constexpr std::uint32_t ln[3][2]{{1, 2}, {3, 4}, {0, 0}};
  constexpr std::uint32_t sm[3][2]{{5, 5}, {6, 7}, {0, 0}};
  return act == kLayerNorm ? ln[phase][last_block] : sm[phase][last_block];
}

} // namespace

LoopMatmulStC::LoopMatmulStC(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, j, i, idle, loop_id);
  UPDATE(updateCommand)
      .reads(state_Q_, ex_k, ex_j, ex_i, ex_completed, st_utilization_at_limit)
      .writes(cmd_val, cmd_bits);
  UPDATE(updateNextState)
      .reads(state_Q_, req_val, req_rdy, req_bits, cmd_val, cmd_rdy)
      .writes(state_D_);
}

void LoopMatmulStC::updateStatus() {
  const auto state = *state_Q_;
  req_rdy = bit(state.phase == Phase::Idle);
  j = state.j;
  i = state.i;
  idle = bit(state.phase == Phase::Idle);
  loop_id = state.req.loop_id;
}

void LoopMatmulStC::updateCommand() {
  const auto state = *state_Q_;
  const auto& req = state.req;
  const auto current_i = static_cast<std::uint16_t>(state.i);
  const auto current_j = static_cast<std::uint16_t>(state.j);
  const auto max_blocks = maxBlocks(req);
  const auto blocks = static_cast<std::uint16_t>(
      current_j + max_blocks <= req.max_j ? max_blocks : req.max_j - current_j);
  const auto cols = blocks * kDim -
                    (current_j + blocks >= req.max_j ? req.pad_j : 0);
  const auto rows = kDim - (current_i == req.max_i - 1 ? req.pad_i : 0);

  const auto ex_k_value = static_cast<std::uint16_t>(*ex_k);
  const auto ex_j_value = static_cast<std::uint16_t>(*ex_j);
  const auto ex_i_value = static_cast<std::uint16_t>(*ex_i);
  bool ex_ahead = ex_completed == 1 ||
                  (!isNormalization(req.act) &&
                   ex_k_value == req.max_k - 1 &&
                   (ex_j_value >= current_j + blocks ||
                    (ex_j_value == current_j + blocks - 1 && ex_i_value > current_i)));
  if (req.is_resadd == 1) {
    ex_ahead = ex_completed == 1 || ex_i_value > current_i ||
               (ex_i_value == current_i && ex_j_value >= current_j + blocks);
  }
  cmd_val = bit(state.phase != Phase::Idle && st_utilization_at_limit == 0 &&
                ex_ahead && req.dram_addr != 0);
  cmd_bits = SmeshCmd{};
  if (state.phase == Phase::Idle) return;

  const auto acc_row = static_cast<std::uint32_t>(
      std::uint64_t{req.addr_start} +
      (std::uint64_t{current_i} * req.max_j + current_j) * kDim);
  const auto element_bytes = req.full_c == 1 ? sizeof(Acc) : sizeof(Elem);
  const auto offset = ((std::uint64_t{current_i} * req.dram_stride + current_j) *
                       kDim * element_bytes) & 0xffffffffull;

  SmeshCmd command{};
  if (state.phase == Phase::NormConfig) {
    command.funct = static_cast<std::uint32_t>(SmeshFunct::Config);
    command.rs1 = kConfigNorm | (std::uint64_t{state.ln_stat_id} << 8) |
                  (std::uint64_t{1} << 17);
  } else if (state.phase == Phase::NormStore) {
    const auto ln_r = static_cast<std::uint16_t>(state.ln_row + state.ln_stat_id);
    const auto ln_acc_row = static_cast<std::uint32_t>(
        std::uint64_t{req.addr_start} +
        (std::uint64_t{current_i} * req.max_j + current_j) * kDim + ln_r);
    const auto ln_offset = (((std::uint64_t{current_i} * req.dram_stride + current_j) *
                             kDim + std::uint64_t{ln_r} * req.dram_stride) *
                            sizeof(Elem)) & 0xffffffffull;
    const bool last_block = current_j + max_blocks >= req.max_j;
    command.funct = static_cast<std::uint32_t>(SmeshFunct::Mvout);
    command.rs1 = req.dram_addr + ln_offset;
    command.rs2 = packLocal(
        makeAccAddr(ln_acc_row, false, req.full_c == 1,
                    normCmd(req.act, state.ln_cmd, last_block)), {1, cols});
  } else {
    command.funct = static_cast<std::uint32_t>(SmeshFunct::Mvout);
    command.rs1 = req.dram_addr + offset;
    command.rs2 = packLocal(makeAccAddr(acc_row, false, req.full_c == 1), {rows, cols});
  }
  cmd_bits = command;
}

void LoopMatmulStC::updateNextState() {
  const auto current = *state_Q_;
  if (current.phase == Phase::Idle) {
    if (req_val == 1 && req_rdy == 1) {
      auto next = current;
      next.req = *req_bits;
      next.i = 0;
      next.j = 0;
      next.ln_row = 0;
      next.ln_cmd = 0;
      next.ln_stat_id = 0;
      next.phase = isNormalization(next.req.act) ? Phase::NormConfig : Phase::Store;
      state_D_ = next;
      trace("lmStC: accepted loop=%u I=%u J=%u act=%u",
            static_cast<unsigned>(next.req.loop_id), next.req.max_i,
            next.req.max_j, next.req.act);
    }
    return;
  }

  if (current.req.dram_addr == 0) {
    auto next = current;
    next.phase = Phase::Idle;
    state_D_ = next;
    return;
  }
  if (cmd_val == 0 || cmd_rdy == 0) return;

  auto next = current;
  const auto max_blocks = maxBlocks(current.req);
  if (current.phase == Phase::Store) {
    const auto next_i = floorAdd(current.i, 1, current.req.max_i);
    const auto next_j = next_i == 0
                            ? floorAdd(current.j, max_blocks, current.req.max_j)
                            : static_cast<std::uint16_t>(current.j);
    next.i = next_i;
    next.j = next_j;
    if (next_i == 0 && next_j == 0) next.phase = Phase::Idle;
  } else if (current.phase == Phase::NormConfig) {
    next.phase = Phase::NormStore;
  } else {
    const auto rows = static_cast<std::uint16_t>(
        kDim - (current.i == current.req.max_i - 1 ? current.req.pad_i : 0));
    const auto remaining = static_cast<std::uint16_t>(rows - current.ln_row);
    const auto stat_ids = std::min<std::uint16_t>(remaining, kNormStatIds);
    const auto next_j = floorAdd(current.j, max_blocks, current.req.max_j);
    const auto next_stat_id = next_j == 0
                                  ? floorAdd(current.ln_stat_id, 1, stat_ids)
                                  : static_cast<std::uint16_t>(current.ln_stat_id);
    const auto next_cmd = next_j == 0 && next_stat_id == 0
                              ? floorAdd(current.ln_cmd, 1, kNormCmds)
                              : static_cast<std::uint16_t>(current.ln_cmd);
    const auto next_row = next_j == 0 && next_stat_id == 0 && next_cmd == 0
                              ? floorAdd(current.ln_row, kNormStatIds, rows)
                              : static_cast<std::uint16_t>(current.ln_row);
    const auto next_i = next_j == 0 && next_stat_id == 0 && next_cmd == 0 && next_row == 0
                            ? floorAdd(current.i, 1, current.req.max_i)
                            : static_cast<std::uint16_t>(current.i);
    next.j = next_j;
    next.ln_stat_id = next_stat_id;
    next.ln_cmd = next_cmd;
    next.ln_row = next_row;
    next.i = next_i;
    if (next_i == 0 && next_row == 0 && next_cmd == 0 &&
        next_stat_id == 0 && next_j == 0) {
      next.phase = Phase::Idle;
    } else if (next_j == 0) {
      next.phase = Phase::NormConfig;
    }
  }
  state_D_ = next;
  trace("lmStC: sent loop=%u phase=%u ij={%u,%u}",
        static_cast<unsigned>(current.req.loop_id),
        static_cast<unsigned>(current.phase),
        static_cast<unsigned>(current.i), static_cast<unsigned>(current.j));
}

void LoopMatmulStC::reset() {
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
