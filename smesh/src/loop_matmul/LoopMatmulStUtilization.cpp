// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulStUtilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopMatmulStUtilization.hpp"

#include <cassert>
#include <cstdint>

namespace smesh {

LoopMatmulStUtilization::LoopMatmulStUtilization(std::string /*name*/, IMPL_CTOR) {
  count_Q_ <= count_D_;
  UPDATE(updateStatus).reads(count_Q_).writes(outstanding, st_utilization_at_limit);
  UPDATE(updateNextState)
      .reads(count_Q_, st_c_cmd_fire, st_c_spad_cmd_fire, st_completed)
      .writes(count_D_);
}

// Report the current number of commands awaiting RS completion.
void LoopMatmulStUtilization::updateStatus() {
  const auto count = static_cast<std::uint16_t>(*count_Q_);
  outstanding = count;
  st_utilization_at_limit = bit(count >= kDefaultConfig.rs_store_entries);
}

// Count accepted commands and subtract completions for the next clock cycle.
void LoopMatmulStUtilization::updateNextState() {
  const auto count = static_cast<std::uint16_t>(*count_Q_);
  const auto completed = static_cast<std::uint8_t>(*st_completed);
  // The shared command arbiter accepts at most one store command per cycle.
  const unsigned issued = st_c_cmd_fire == 1 || st_c_spad_cmd_fire == 1;
  assert(completed <= count);
  const auto next = count + issued - completed;
  assert(next <= kDefaultConfig.rs_store_entries);
  if (next != count) {
    count_D_ = static_cast<std::uint16_t>(next);
    trace("st utilization: issued=%u completed=%u count=%u",
          issued, static_cast<unsigned>(completed), next);
  }
}

void LoopMatmulStUtilization::reset() {
  count_D_.reset(0);
  outstanding.reset(0);
  st_utilization_at_limit.reset(0);
}

} // namespace smesh
