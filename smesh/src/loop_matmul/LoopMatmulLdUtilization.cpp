// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulLdUtilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulLdUtilization.hpp"

#include <cassert>
#include <cstdint>

namespace smesh {

LoopMatmulLdUtilization::LoopMatmulLdUtilization(std::string /*name*/, IMPL_CTOR) {
  count_Q_ <= count_D_;
  UPDATE(updateStatus).reads(count_Q_).writes(outstanding, ld_utilization_at_limit);
  UPDATE(updateNextState)
      .reads(count_Q_, lda_cmd_fire, ldb_cmd_fire, ldd_cmd_fire, ld_completed)
      .writes(count_D_);
}

void LoopMatmulLdUtilization::updateStatus() {
  const auto count = static_cast<std::uint16_t>(*count_Q_);
  outstanding = count;
  ld_utilization_at_limit = bit(count >= kDefaultConfig.rs_load_entries);
}

void LoopMatmulLdUtilization::updateNextState() {
  const auto count = static_cast<std::uint16_t>(*count_Q_);
  const auto completed = static_cast<std::uint8_t>(*ld_completed);
  // The shared command arbiter allows at most one load command to fire per cycle.
  const auto issued = lda_cmd_fire == 1 || ldb_cmd_fire == 1 || ldd_cmd_fire == 1;
  assert(completed <= count);
  const auto next = count + static_cast<unsigned>(issued) - completed;
  assert(next <= kDefaultConfig.rs_load_entries);
  if (next != count) {
    count_D_ = static_cast<std::uint16_t>(next);
    trace("ld utilization: issued=%u completed=%u count=%u",
          static_cast<unsigned>(issued), static_cast<unsigned>(completed), next);
  }
}

void LoopMatmulLdUtilization::reset() {
  count_D_.reset(0);
  outstanding.reset(0);
  ld_utilization_at_limit.reset(0);
}

} // namespace smesh
