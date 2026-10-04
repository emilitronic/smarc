// **********************************************************************
// smesh/src/accum_response/NormStats.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormStats.hpp"

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace smesh {

NormStats::NormStats(std::string /*name*/, IMPL_CTOR) {
  regs_Q_ <= regs_D_;
  UPDATE(updateView).reads(regs_Q_).writes(view);
  UPDATE(updateState).reads(regs_Q_, accept_val, accept_id, req_bits, slot_states,
                            sum_chunk_val, sum_chunk_bits, max_chunk_val)
                     .reads(max_chunk_bits, sum_val, sum_bits, max_val, max_bits)
                     .writes(regs_D_);
}

void NormStats::updateView() {
  view = *regs_Q_;
}

void NormStats::updateState() {
  auto next    = *regs_Q_;
  bool changed = false;

  const auto issueChunk = [&](const NormChunk& chunk) {
    const auto id = static_cast<std::size_t>(static_cast<std::uint8_t>(chunk.slot));
    assert(id < kNormStatsSlots);
    const auto left = static_cast<std::uint16_t>(next.elems_left[id]);
    const auto len = static_cast<std::uint16_t>(chunk.len);
    assert(len <= left);
    next.elems_left[id] = u16(left - len);
    changed = true;
  };
  if (sum_chunk_val == 1) issueChunk(*sum_chunk_bits);
  if (max_chunk_val == 1) issueChunk(*max_chunk_bits);

  if (sum_val == 1) {
    const auto result = *sum_bits;
    const auto id     = static_cast<std::size_t>(static_cast<std::uint8_t>(result.slot));
    assert(id < kNormStatsSlots);
    next.sum[id] = u32(static_cast<std::uint32_t>(next.sum[id]) + static_cast<std::uint32_t>(result.sum));
    changed = true;
  }

  if (max_val == 1) {
    const auto result = *max_bits;
    const auto id = static_cast<std::size_t>(static_cast<std::uint8_t>(result.slot));
    assert(id < kNormStatsSlots);
    const auto maximum = std::max(next.running_max[id], result.max);
    next.running_max[id] = maximum;
    next.max[id] = maximum;
    changed = true;
  }

  const auto states = *slot_states;
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state = static_cast<NormFsmState>(static_cast<std::uint8_t>(states.state[id]));
    if (state == NormFsmState::Output) {
      next.sum[id]   = 0;
      next.count[id] = 0;
      next.running_max[id] = std::numeric_limits<Acc>::min();
      changed        = true;
    }
  }

  if (accept_val == 1) {
    const auto id  = static_cast<std::size_t>(static_cast<std::uint8_t>(*accept_id));
    assert(id < kNormStatsSlots);
    const auto req = *req_bits;
    const auto cmd = static_cast<NormCmd>(static_cast<std::uint8_t>(req.cmd.cmd));
    if (cmd == NormCmd::Reset) {
      next.elems_left[id] = 0;
    } else if (cmd == NormCmd::Sum || cmd == NormCmd::Max) {
      const auto len      = static_cast<std::uint16_t>(req.cmd.len);
      next.count[id]      = u16(static_cast<std::uint16_t>(next.count[id]) + len);
      next.elems_left[id] = u16(len);
    }
    changed = true;
  }

  if (changed) regs_D_ = next;
}

void NormStats::reset() {
  regs_Q_.reset(NormStatsRegs{});
  regs_D_.reset(NormStatsRegs{});
  view.reset(NormStatsRegs{});
}

} // namespace smesh
