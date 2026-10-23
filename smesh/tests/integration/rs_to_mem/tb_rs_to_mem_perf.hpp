// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_perf.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#pragma once

#include "SmeshPerfCounters.hpp"

namespace smesh {
namespace tb {

/* Accumulates timing statistics from one sample per cycle after Sim::run().
   Execution includes the first accepted input command and the cycle
   activityComplete() first becomes true.
   Only subsequent completion cycles count as drain; reset/setup are excluded. */
class RsMemPerfStats {
 public:
  void reset();                                                     // Clear all counts and saved start/completion cycle numbers.
  void sample(bool input_command_accepted, bool workload_complete); // Record one cycle using the two answers supplied by the runner.
  void report(const std::string& scenario) const;                   // Print the accumulated measurements under this test's name.
  const SmeshPerfCounters& counters() const { return counters_; }   // Give callers read access to the accumulated measurements.
  bool started() const { return first_input_cycle_ != 0; }          // Has the runner reported an accepted input command?
  bool completed() const { return completion_cycle_ != 0; }         // Has the runner reported completion, with no activity resuming since?

 private:
  SmeshPerfCounters counters_;
  std::uint64_t first_input_cycle_ = 0;
  std::uint64_t completion_cycle_ = 0;
};

} // namespace tb
} // namespace smesh
