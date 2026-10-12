// **********************************************************************
// smesh/src/loop_conv/LoopConvLdWeight.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvLdWeight.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cassert>

namespace smesh {
namespace {

constexpr std::uint32_t kMaxChannels = kDim * std::max<std::size_t>(1, kDefaultConfig.dma_max_bytes / (kDim * sizeof(Elem)));

// Original's floorAdd wraps at the command bound, not at the register width.
std::uint16_t floorAdd(std::uint16_t value, std::uint32_t step, std::uint16_t limit, bool enable = true) {
  if (!enable) return value;
  return std::uint32_t{value} + step >= limit ? 0 : static_cast<std::uint16_t>(value + step);
}

// DRAM distance between consecutive rows of the selected weight layout.
std::uint64_t dramStride(const LoopConvLdWeightReq& req) {
  const auto& outer = req.outer_bounds;
  if (req.dw == 1) return sizeof(Elem);
  if (req.trans_weight_1203 == 1)
    return std::uint64_t{outer.kernel_dim} * outer.kernel_dim * outer.out_channels * sizeof(Elem);
  if (req.trans_weight_0132 == 1) return std::uint64_t{outer.in_channels} * sizeof(Elem);
  return std::uint64_t{outer.weight_stride} * sizeof(Elem);
}

// Capture one LOAD2's addresses and shape from the current four coordinates.
SmeshCmd loadCommand(const LoopConvLdWeightReq& req, std::uint16_t och, std::uint16_t krow, std::uint16_t kcol, std::uint16_t kch) {
  const auto& outer      = req.outer_bounds;
  const auto& inner      = req.inner_bounds;
  const auto& geometry   = req.geometry;
  const bool transposed  = req.trans_weight_0132 == 1;
  const auto groups      = transposed ? geometry.in_channels_per_bank : geometry.out_channels_per_bank;
  const auto channels    = transposed ? geometry.ochs : inner.kchs;
  const auto weight_rows = std::uint64_t{groups} * inner.kcols * inner.krows * channels;
  // Unsigned subtraction preserves the low address bits, including Original's
  // wrap behavior. Legal software setups must allocate enough SPAD for weights.
  const auto start = std::uint64_t{req.addr_end} - weight_rows;
  const auto block = transposed ? kch / kDim : och / kDim;
  const auto row   = transposed ? och : kch;
  const auto local = start + std::uint64_t{block} * inner.krows * inner.kcols * channels +
                     std::uint64_t{krow} * inner.kcols * channels + std::uint64_t{kcol} * channels + row;

  std::uint64_t offset;
  // Original gives dw precedence over 1203, then 0132, for DRAM addressing only.
  if (req.dw == 1) {
    offset = std::uint64_t{krow} * outer.kernel_dim + kcol;
  } else if (req.trans_weight_1203 == 1) {
    offset = (std::uint64_t{kch} * outer.kernel_dim * outer.kernel_dim +
              std::uint64_t{krow} * outer.kernel_dim + kcol) * outer.out_channels + och;
  } else if (transposed) {
    offset = (std::uint64_t{krow} * outer.kernel_dim * outer.out_channels +
              std::uint64_t{kcol} * outer.out_channels + och) * outer.in_channels + kch;
  } else {
    offset = (std::uint64_t{krow} * outer.kernel_dim * outer.in_channels +
              std::uint64_t{kcol} * outer.in_channels + kch) * outer.weight_stride + och;
  }
  const auto cols = std::min<std::uint32_t>(transposed ? inner.kchs - kch : geometry.ochs - och,
                                          kMaxChannels);
  const auto rows = std::min<std::uint32_t>(transposed ? geometry.ochs - och : inner.kchs - kch, kDim);
  SmeshCmd command{};
  command.funct = static_cast<std::uint32_t>(SmeshFunct::Mvin2);
  command.rs1   = req.dram_addr + ((offset * sizeof(Elem)) & 0xffffffffull);
  command.rs2   = packLocal(makeSpAddr(static_cast<std::uint32_t>(local)), {rows, cols});
  return command;
}

} // namespace

LoopConvLdWeight::LoopConvLdWeight(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, idle, loop_id);
  UPDATE(updateCommand).reads(state_Q_, ld_utilization_at_limit).writes(cmd_val, cmd_bits);
  UPDATE(updateNextState).reads(state_Q_, req_val, req_bits, req_rdy, cmd_rdy, ld_utilization_at_limit, wait_for_prev_loop)
      .writes(state_D_);
}

// Accept another job only after generation ends and both command stages drain.
void LoopConvLdWeight::updateStatus() {
  const auto state = *state_Q_;
  const bool empty = state.phase == Phase::Idle && state.valid[0] == 0 && state.valid[1] == 0;
  req_rdy          = bit(empty);
  idle             = bit(empty);
  loop_id          = state.req.loop_id;
}

// Send the pipeline's last command unless RS load pressure blocks it.
void LoopConvLdWeight::updateCommand() {
  const auto state = *state_Q_;
  cmd_bits         = state.valid[1] == 1 ? state.commands[1] : SmeshCmd{};
  cmd_val          = bit(state.valid[1] == 1 && ld_utilization_at_limit == 0);
}

// Advance pipeline entries and weight coordinates only when their stage accepts.
void LoopConvLdWeight::updateNextState() {
  const auto current      = *state_Q_;
  auto next               = current;
  const bool output_ready = cmd_rdy == 1 && ld_utilization_at_limit == 0;
  const bool stage1_ready = current.valid[1] == 0 || output_ready;
  const bool stage0_ready = current.valid[0] == 0 || stage1_ready;
  if (stage1_ready) {
    next.valid[1]    = current.valid[0];
    next.commands[1] = current.valid[0] == 1 ? current.commands[0] : SmeshCmd{};
  }
  if (stage0_ready) {
    next.valid[0]   = 0;
    next.commands[0] = {};
  }

  const auto& req = current.req;
  if (req.dram_addr == 0) next.phase = Phase::Idle;
  const bool enqueue = current.phase != Phase::Idle && req.dram_addr != 0 && wait_for_prev_loop == 0 && stage0_ready;
  if (enqueue) {
    SmeshCmd command{};
    if (current.phase == Phase::Config) {
      command.funct = static_cast<std::uint32_t>(SmeshFunct::Config);
      command.rs1   = packConfigLoadRs1(1, req.geometry.weight_spad_stride, kMvinScaleIdentityBits, false, 1);
      command.rs2   = dramStride(req);
      next.phase    = Phase::Load;
    } else {
      command               = loadCommand(req, current.och, current.krow, current.kcol, current.kch);
      const bool transposed = req.trans_weight_0132 == 1;
      const auto max_chs    = std::min<std::uint32_t>(transposed ? req.inner_bounds.kchs : req.geometry.ochs, kMaxChannels);
      next.kch  = floorAdd(current.kch, transposed ? max_chs : kDim, req.inner_bounds.kchs);
      next.kcol = floorAdd(current.kcol, 1, req.inner_bounds.kcols, next.kch == 0);
      next.krow = floorAdd(current.krow, 1, req.inner_bounds.krows, next.kcol == 0 && next.kch == 0);
      next.och  = floorAdd(current.och, transposed ? kDim : max_chs, req.geometry.ochs, next.krow == 0 && next.kcol == 0 && next.kch == 0);
      if (next.och == 0 && next.krow == 0 && next.kcol == 0 && next.kch == 0)
        next.phase = Phase::Idle;
    }
    // Encode at pipeline entry: the captured request and coordinates cannot change.
    next.commands[0] = command;
    next.valid[0]    = 1;
    trace("loop_conv weights: queued funct=%u loop=%u och=%u krow=%u kcol=%u kch=%u",
          static_cast<unsigned>(command.funct), unsigned(req.loop_id),
          current.och, current.krow, current.kcol, current.kch);
  }
  if (req_val == 1 && req_rdy == 1) {
    next.req = *req_bits;
    if (next.req.dram_addr != 0) {
      assert(next.req.geometry.ochs > 0 && next.req.inner_bounds.kchs > 0 &&
             next.req.inner_bounds.krows > 0 && next.req.inner_bounds.kcols > 0);
    }
    next.phase = Phase::Config;
    next.och   = next.krow = next.kcol = next.kch = 0;
    trace("loop_conv weights: accepted loop=%u", unsigned(next.req.loop_id));
  }
  if (current.valid[1] == 1 && output_ready)
    trace("loop_conv weights: sent funct=%u loop=%u", static_cast<unsigned>(current.commands[1].funct), unsigned(req.loop_id));
  state_D_ = next;
}

void LoopConvLdWeight::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  idle.reset(1);
  loop_id.reset(0);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
}

} // namespace smesh
