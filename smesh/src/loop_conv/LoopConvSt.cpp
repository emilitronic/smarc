// **********************************************************************
// smesh/src/loop_conv/LoopConvSt.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvSt.hpp"
#include "SmeshCommand.hpp"
#include <algorithm>
#include <cassert>

namespace smesh {
namespace {

std::uint16_t floorAdd(std::uint16_t value, std::uint32_t step, std::uint16_t limit,
                       bool enable = true) {
  if (!enable) return value;
  return std::uint32_t{value} + step >= limit ? 0 : static_cast<std::uint16_t>(value + step);
}

// CONFIG_STORE fields are narrower than the loop bounds: explicitly retain
// their low bits. The post-pooling command clears Original's DontCare fields.
SmeshCmd poolConfig(const LoopConvStReq& req, bool before) {
  SmeshCmd cmd{};
  cmd.funct = static_cast<std::uint32_t>(SmeshFunct::Config);
  std::uint64_t rs1 = static_cast<std::uint32_t>(ConfigKind::Store) |
                      (std::uint64_t{req.activation & 3u} << 2);
  if (before) {
    const auto& outer = req.outer_bounds;
    const auto& inner = req.inner_bounds;
    rs1 |= (std::uint64_t{outer.pool_stride & 3u} << 4) |
           (std::uint64_t{outer.pool_size & 3u} << 6) |
           (std::uint64_t{inner.pupad & 3u} << 8) |
           (std::uint64_t{inner.plpad & 3u} << 10) |
           (std::uint64_t{outer.pool_out_col_dim & 0xffu} << 24) |
           (std::uint64_t{inner.porows & 0xffu} << 32) |
           (std::uint64_t{inner.pocols & 0xffu} << 40) |
           (std::uint64_t{inner.orows & 0xffu} << 48) |
           (std::uint64_t{inner.ocols & 0xffu} << 56);
  }
  cmd.rs1 = rs1;
  // All-one scale means "keep the current scale", not an arithmetic multiplier.
  cmd.rs2 = 0xffffffff00000000ull |
            (std::uint64_t{req.outer_bounds.out_stride} * sizeof(Elem) & 0xffffffffull);
  return cmd;
}

} // namespace

LoopConvSt::LoopConvSt(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateStatus).reads(state_Q_).writes(req_rdy, idle, loop_id);
  UPDATE(updateCommand).reads(state_Q_, st_utilization_at_limit).writes(cmd_val, cmd_bits);
  UPDATE(updateNextState).reads(state_Q_, req_val, req_bits, req_rdy, cmd_rdy, ex_completed, st_utilization_at_limit)
      .writes(state_D_);
}

// Accept a new job only after generation ends and both command stages drain.
void LoopConvSt::updateStatus() {
  const auto q = *state_Q_;
  const bool empty = q.phase == Phase::Idle && q.valid[0] == 0 && q.valid[1] == 0;
  req_rdy = bit(empty);
  idle = bit(empty);
  loop_id = q.req.loop_id;
}

// Hold the saved output command while downstream or RS store pressure stalls it.
void LoopConvSt::updateCommand() {
  const auto q = *state_Q_;
  cmd_val = bit(q.valid[1] == 1 && st_utilization_at_limit == 0);
  cmd_bits = q.valid[1] == 1 ? q.commands[1] : SmeshCmd{};
}

// Capture the addresses and shapes that belong to this pipeline entry.
SmeshCmd LoopConvSt::makeCommand(const State& q) {
  const auto& req = q.req;
  if (q.phase == Phase::PrePoolConfig) return poolConfig(req, true);
  if (q.phase == Phase::PostPoolConfig) return poolConfig(req, false);
  const auto& outer = req.outer_bounds;
  const auto& inner = req.inner_bounds;
  const auto cols = std::min<std::uint32_t>(req.ochs - q.och, kDim);
  const auto base = std::uint64_t{req.addr_start} +
      (q.och / kDim) * std::uint64_t{inner.batches} * inner.orows * inner.ocols +
      std::uint64_t{q.b} * inner.orows * inner.ocols;
  SmeshCmd cmd{};
  cmd.funct = static_cast<std::uint32_t>(SmeshFunct::Mvout);
  if (q.phase == Phase::Pool) {
    // Unlike ordinary stores, Original does not mask this offset to 32 bits.
    const auto offset = (std::uint64_t{q.b} * outer.pool_out_col_dim * outer.pool_out_row_dim *
                         outer.out_stride + q.och) * sizeof(Elem);
    cmd.rs1 = req.dram_addr + offset;
    // StCtrl uses the configured pooling geometry, not this row count.
    // Original leaves num_rows unspecified; use zero deterministically.
    cmd.rs2 = packLocal(makeAccAddr(static_cast<std::uint32_t>(base), false, false), {0, cols});
  } else {
    const auto offset = req.trans_output_1203 == 1
        ? ((std::uint64_t{q.orow} * outer.out_col_dim * outer.batch_size +
            std::uint64_t{q.ocol} * outer.batch_size + q.b) * outer.out_channels + q.och) * sizeof(Elem)
        : ((std::uint64_t{q.b} * outer.out_row_dim * outer.out_col_dim +
            std::uint64_t{q.orow} * outer.out_col_dim + q.ocol) * outer.out_stride + q.och) * sizeof(Elem);
    const auto row = base + std::uint64_t{q.orow} * inner.ocols + q.ocol;
    const auto rows = std::min<std::uint32_t>(inner.ocols - q.ocol, kDim);
    cmd.rs1 = req.dram_addr + (offset & 0xffffffffull);
    cmd.rs2 = packLocal(makeAccAddr(static_cast<std::uint32_t>(row), false, false), {rows, cols});
  }
  return cmd;
}

// Move the elastic pipeline and walk counters only on command enqueue.
// Execute completion gates new generation, not already-buffered output commands.
void LoopConvSt::updateNextState() {
  const auto q = *state_Q_;
  auto next = q;
  const bool output_ready = cmd_rdy == 1 && st_utilization_at_limit == 0;
  const bool stage1_ready = q.valid[1] == 0 || output_ready;
  const bool stage0_ready = q.valid[0] == 0 || stage1_ready;
  if (stage1_ready) {
    next.valid[1] = q.valid[0];
    next.commands[1] = q.valid[0] == 1 ? q.commands[0] : SmeshCmd{};
  }
  if (stage0_ready) {
    next.valid[0] = 0;
    next.commands[0] = {};
  }
  const auto& req = q.req;
  if (req.dram_addr == 0) next.phase = Phase::Idle;
  const bool enqueue = q.phase != Phase::Idle && req.dram_addr != 0 && ex_completed == 1 && stage0_ready;
  if (enqueue) {
    next.commands[0] = makeCommand(q);
    next.valid[0] = 1;
    if (req.no_pool == 1) {
      next.och = floorAdd(q.och, kDim, req.ochs);
      next.ocol = floorAdd(q.ocol, kDim, req.inner_bounds.ocols, next.och == 0);
      next.orow = floorAdd(q.orow, 1, req.inner_bounds.orows, next.ocol == 0 && next.och == 0);
      next.b = floorAdd(q.b, 1, req.inner_bounds.batches, next.orow == 0 && next.ocol == 0 && next.och == 0);
      next.phase = next.b == 0 && next.orow == 0 && next.ocol == 0 && next.och == 0 ? Phase::Idle : Phase::Store;
    } else if (q.phase == Phase::PrePoolConfig) next.phase = Phase::Pool;
    else if (q.phase == Phase::PostPoolConfig) next.phase = Phase::Idle;
    else {
      next.och = floorAdd(q.och, kDim, req.ochs);
      next.b = floorAdd(q.b, 1, req.inner_bounds.batches, next.och == 0);
      next.phase = next.b == 0 && next.och == 0 ? Phase::PostPoolConfig : Phase::Pool;
    }
    trace("loop_conv store: queued funct=%u loop=%u phase=%u b=%u row=%u col=%u och=%u",
          unsigned(next.commands[0].funct), unsigned(req.loop_id), unsigned(q.phase), q.b, q.orow, q.ocol, q.och);
  }
  if (req_val == 1 && req_rdy == 1) {
    next.req = *req_bits;
    if (next.req.dram_addr != 0) {
      assert(next.req.ochs > 0 && next.req.inner_bounds.batches > 0);
      if (next.req.no_pool == 1) assert(next.req.inner_bounds.orows > 0 && next.req.inner_bounds.ocols > 0);
    }
    next.phase = next.req.no_pool == 1 ? Phase::Store : Phase::PrePoolConfig;
    next.b = next.orow = next.ocol = next.och = 0;
    trace("loop_conv store: accepted loop=%u", unsigned(next.req.loop_id));
  }
  if (q.valid[1] == 1 && output_ready)
    trace("loop_conv store: sent funct=%u loop=%u", unsigned(q.commands[1].funct), unsigned(req.loop_id));
  state_D_ = next;
}

void LoopConvSt::reset() {
  state_D_.reset(State{});
  req_rdy.reset(1);
  idle.reset(1);
  loop_id.reset(0);
  cmd_val.reset(0);
  cmd_bits.reset(SmeshCmd{});
}

} // namespace smesh
