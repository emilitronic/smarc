// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulExUtilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopMatmulExUtilization.hpp"

#include <cassert>
#include <cstdint>

namespace smesh {

LoopMatmulExUtilization::LoopMatmulExUtilization(std::string /*name*/, IMPL_CTOR) {
  count_Q_ <= count_D_;
  UPDATE(updateStatus).reads(count_Q_).writes(outstanding, ex_utilization_at_limit);
  UPDATE(updateNextState)
      .reads(count_Q_, ex_cmd_fire, ex_completed)
      .writes(count_D_);
}

// Report the current number of commands awaiting RS completion.
void LoopMatmulExUtilization::updateStatus() {
  const auto count = static_cast<std::uint16_t>(*count_Q_);
  outstanding = count;
  ex_utilization_at_limit = bit(count >= kDefaultConfig.rs_execute_entries);
}

// Count accepted commands and subtract completions for the next clock cycle.
void LoopMatmulExUtilization::updateNextState() {
  const auto count = static_cast<std::uint16_t>(*count_Q_);
  const auto completed = static_cast<std::uint8_t>(*ex_completed);
  const unsigned issued = ex_cmd_fire == 1;
  assert(completed <= count);
  const auto next = count + issued - completed;
  assert(next <= kDefaultConfig.rs_execute_entries);
  if (next != count) {
    count_D_ = static_cast<std::uint16_t>(next);
    trace("ex utilization: issued=%u completed=%u count=%u",
          issued, static_cast<unsigned>(completed), next);
  }
}

void LoopMatmulExUtilization::reset() {
  count_D_.reset(0);
  outstanding.reset(0);
  ex_utilization_at_limit.reset(0);
}

} // namespace smesh
