// **********************************************************************
// smesh/src/loop_conv/LoopConvLdInput.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvLdInput.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cassert>

namespace smesh {
namespace {

static_assert((kDim & (kDim - 1)) == 0, "Original's input addressing requires a power-of-two DIM");
constexpr std::uint32_t kMaxElements = kDim *
    std::max<std::size_t>(1, kDefaultConfig.dma_max_bytes / (kDim * sizeof(Elem)));

// Explicit signed-16 register truncation, without an implementation-defined cast.
std::int16_t signed16(std::int32_t value) {
  const auto low = static_cast<std::uint16_t>(value);
  return static_cast<std::int16_t>(low <= 32767 ? std::int32_t{low} : std::int32_t{low} - 65536);
}

std::int32_t undilated(std::uint16_t value, bool dilated) {
  return (std::uint32_t{value} + unsigned(dilated)) >> unsigned(dilated);
}

// Signed arithmetic shift expressed with division so negative values round down.
std::int64_t shifted(std::int64_t value, unsigned shift) {
  const auto divisor = std::int64_t{1} << shift;
  return value >= 0 ? value / divisor : (value - divisor + 1) / divisor;
}

// Keep the wider next value for carry/wrap decisions; truncate only at registers.
std::int32_t floorAdd(std::int32_t value, std::int32_t step,
                      std::int32_t limit, std::int32_t minimum, bool enable = true) {
  if (!enable) return value;
  return value + step >= limit ? minimum : value + step;
}

struct InputCommand {
  SmeshCmd bits{};
  std::int32_t columns_step = 0;
  std::int32_t channels_step = 0;
};

// Build one LOAD command from the saved request and current signed coordinates.
InputCommand loadCommand(const LoopConvLdInputReq& req, std::int16_t b,
                         std::int16_t irow, std::int16_t icol, std::int16_t ich) {
  const auto& outer = req.outer_bounds;
  const auto& inner = req.inner_bounds;
  const auto& geometry = req.geometry;
  const bool transposed = req.trans_input_3120 == 1;
  const bool dilated = req.input_dilated == 1;
  const unsigned shift = req.downsample == 1 ? 1 : 0;
  const auto left = undilated(inner.lpad, dilated);
  const auto right = undilated(inner.rpad, dilated);
  const auto top = undilated(inner.upad, dilated);
  const bool zeros = irow < 0 || irow >= geometry.irows_unpadded ||
                     icol < 0 || icol >= geometry.icols_unpadded;
  const auto channels_step = static_cast<std::int32_t>(
      std::min<std::uint32_t>(transposed ? inner.batches : geometry.ichs, kMaxElements));
  const auto columns_step = icol < 0
      ? std::min<std::int32_t>(-std::int32_t{icol}, kDim)
      : icol >= geometry.icols_unpadded
          ? std::min<std::int32_t>(geometry.icols_unpadded + right - icol, kDim)
          : std::min<std::int32_t>(geometry.icols_unpadded - icol, kDim << shift);
  const auto cols = std::min<std::int32_t>(
      (transposed ? inner.batches - b : geometry.ichs - ich), channels_step);
  assert(columns_step > 0 && cols > 0);

  // Only the low 32 offset bits are used. Unsigned arithmetic preserves them
  // even for negative padding coordinates, whose LOAD source is forced to zero.
  const auto batch = static_cast<std::uint64_t>(std::int64_t{b});
  const auto row = static_cast<std::uint64_t>(std::int64_t{irow});
  const auto col = static_cast<std::uint64_t>(std::int64_t{icol});
  const auto channel = static_cast<std::uint64_t>(std::int64_t{ich});
  const auto element_offset = transposed
      ? (channel * outer.in_col_dim * outer.in_row_dim + row * outer.in_col_dim + col) *
            inner.batches + batch
      : (batch * outer.in_row_dim * outer.in_col_dim + row * outer.in_col_dim + col) *
            outer.in_stride + channel;
  const auto dram_offset = (element_offset * sizeof(Elem)) & 0xffffffffull;
  const auto rows_downsampled = geometry.irows >> shift;
  const auto cols_downsampled = geometry.icols >> shift;
  const auto plane = transposed ? ich : b;
  const auto block = transposed ? b : ich;
  const auto local_row = std::int64_t{req.addr_start} +
      shifted(block, log2Up(kDim)) * geometry.input_spad_stride +
      std::int64_t{plane} * rows_downsampled * cols_downsampled +
      shifted(std::int64_t{irow} + top, shift) * cols_downsampled +
      shifted(std::int64_t{icol} + left, shift);
  InputCommand command{};
  command.bits.funct = static_cast<std::uint32_t>(SmeshFunct::Mvin);
  command.bits.rs1 = zeros ? 0 : req.dram_addr + dram_offset;
  command.bits.rs2 = packLocal(makeSpAddr(static_cast<std::uint32_t>(local_row)),
                              {static_cast<std::size_t>(columns_step >> shift),
                               static_cast<std::size_t>(cols)});
  command.columns_step = columns_step;
  command.channels_step = channels_step;
  return command;
}

} // namespace

LoopConvLdInput::LoopConvLdInput(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, idle, loop_id);
  UPDATE(updateCommand).reads(state_Q_, ld_utilization_at_limit).writes(cmd_val, cmd_bits);
  UPDATE(updateNextState)
      .reads(state_Q_, req_val, req_bits, req_rdy, cmd_rdy,
             ld_utilization_at_limit, wait_for_prev_loop)
      .writes(state_D_);
}

// Report idle only when the FSM is idle and all generated commands have drained.
void LoopConvLdInput::updateStatus() {
  const auto state = *state_Q_;
  const bool empty = state.phase == Phase::Idle && state.valid[0] == 0 && state.valid[1] == 0;
  req_rdy = bit(empty);
  idle = bit(empty);
  loop_id = state.req.loop_id;
}

// Present the final pipeline stage; RS pressure gates its output handshake.
void LoopConvLdInput::updateCommand() {
  const auto state = *state_Q_;
  cmd_bits = state.valid[1] == 1 ? state.commands[1] : SmeshCmd{};
  cmd_val = bit(state.valid[1] == 1 && ld_utilization_at_limit == 0);
}

// Move pipeline entries and advance coordinates only when a new command enters.
void LoopConvLdInput::updateNextState() {
  const auto current = *state_Q_;
  auto next = current;
  const bool output_ready = cmd_rdy == 1 && ld_utilization_at_limit == 0;
  const bool stage1_ready = current.valid[1] == 0 || output_ready;
  const bool stage0_ready = current.valid[0] == 0 || stage1_ready;
  if (stage1_ready) {
    next.valid[1] = current.valid[0];
    next.commands[1] = current.valid[0] == 1 ? current.commands[0] : SmeshCmd{};
  }
  if (stage0_ready) {
    next.valid[0] = 0;
    next.commands[0] = {};
  }
  const auto& req = current.req;
  if (req.dram_addr == 0) next.phase = Phase::Idle;
  const bool enqueue = current.phase != Phase::Idle && req.dram_addr != 0 &&
                       wait_for_prev_loop == 0 && stage0_ready;
  if (enqueue) {
    SmeshCmd command{};
    if (current.phase == Phase::Config) {
      command.funct = static_cast<std::uint32_t>(SmeshFunct::Config);
      command.rs1 = packConfigLoadRs1(0, req.geometry.input_spad_stride,
                                     kMvinScaleIdentityBits, false, req.max_pixels_per_row);
      command.rs2 = (std::uint64_t{req.trans_input_3120 == 1
                                      ? req.outer_bounds.batch_size : req.outer_bounds.in_stride} *
                     sizeof(Elem)) << unsigned(req.downsample == 1);
      next.phase = Phase::Load;
    } else {
      const auto load = loadCommand(req, current.b, current.irow, current.icol, current.ich);
      command = load.bits;
      const bool transposed = req.trans_input_3120 == 1;
      const bool dilated = req.input_dilated == 1;
      const auto left = undilated(req.inner_bounds.lpad, dilated);
      const auto top = undilated(req.inner_bounds.upad, dilated);
      const auto next_ich = floorAdd(current.ich, transposed ? 1 : load.channels_step,
                                    req.geometry.ichs, 0);
      const auto next_icol = floorAdd(current.icol, load.columns_step,
          req.geometry.icols_unpadded + undilated(req.inner_bounds.rpad, dilated),
          -left, next_ich == 0);
      const auto next_irow = floorAdd(current.irow, 1 << unsigned(req.downsample == 1),
          req.geometry.irows_unpadded + undilated(req.inner_bounds.dpad, dilated),
          -top, next_icol == -left && next_ich == 0);
      const auto next_b = floorAdd(current.b, transposed ? load.channels_step : 1,
          req.inner_bounds.batches, 0, next_irow == -top && next_icol == -left && next_ich == 0);
      next.ich = signed16(next_ich);
      next.icol = signed16(next_icol);
      next.irow = signed16(next_irow);
      next.b = signed16(next_b);
      if (next_b == 0 && next_irow == -top && next_icol == -left && next_ich == 0)
        next.phase = Phase::Idle;
    }
    // Encode the final command at pipeline entry; captured coordinates cannot change it.
    next.commands[0] = command;
    next.valid[0] = 1;
    trace("loop_conv input: queued funct=%u loop=%u b=%d row=%d col=%d ch=%d",
          static_cast<unsigned>(command.funct), unsigned(req.loop_id),
          current.b, current.irow, current.icol, current.ich);
  }
  if (req_val == 1 && req_rdy == 1) {
    next.req = *req_bits;
    if (next.req.dram_addr != 0) {
      assert(next.req.inner_bounds.batches > 0 && next.req.geometry.ichs > 0);
    }
    const bool dilated = next.req.input_dilated == 1;
    next.phase = Phase::Config;
    next.b = next.ich = 0;
    next.irow = signed16(-undilated(next.req.inner_bounds.upad, dilated));
    next.icol = signed16(-undilated(next.req.inner_bounds.lpad, dilated));
    trace("loop_conv input: accepted loop=%u", unsigned(next.req.loop_id));
  }
  if (current.valid[1] == 1 && output_ready)
    trace("loop_conv input: sent funct=%u loop=%u",
          static_cast<unsigned>(current.commands[1].funct), unsigned(req.loop_id));
  state_D_ = next;
}

void LoopConvLdInput::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  idle.reset(1);
  loop_id.reset(0);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
}

} // namespace smesh
