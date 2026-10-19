// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_perf.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#pragma once

#include "SmeshPerfCounters.hpp"

namespace smesh {
namespace tb {

// Observes one harness cycle after Sim::run(). Execution includes the first
// accepted input command and the cycle activityComplete() first becomes true.
// Only subsequent completion cycles count as drain; reset/setup are excluded.
class RsMemPerfObserver {
 public:
  void reset();
  void sample(bool input_command_accepted, bool workload_complete);
  void report(const std::string& scenario) const;
  const SmeshPerfCounters& counters() const { return counters_; }
  bool started() const { return first_input_cycle_ != 0; }
  bool completed() const { return completion_cycle_ != 0; }

 private:
  SmeshPerfCounters counters_;
  std::uint64_t first_input_cycle_ = 0;
  std::uint64_t completion_cycle_ = 0;
};

} // namespace tb
} // namespace smesh
