// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_perf.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#include "tb_rs_to_mem_perf.hpp"

#include <descore.hpp>
#include <Trace.hpp>

#include <cinttypes>

namespace smesh {
namespace tb {

TraceKey(rs_mem_perf_);

void RsMemPerfStats::reset() {
  counters_.reset();
  first_input_cycle_ = 0;
  completion_cycle_ = 0;
}

void RsMemPerfStats::sample(bool input_command_accepted, bool workload_complete) {
  counters_.add(SmeshPerfEvent::SimulationCycles);
  const auto cycle = counters_.value(SmeshPerfEvent::SimulationCycles);
  if (input_command_accepted) {
    counters_.add(SmeshPerfEvent::InputCommands);
    if (!started()) first_input_cycle_ = cycle;
  }

  if (started()) {
    // A new period of activity cancels the previous completion boundary.
    // Its provisional drain cycles then belong to the execution interval.
    if (!workload_complete) completion_cycle_ = 0;
    else if (!completed()) completion_cycle_ = cycle;

    const auto end = completed() ? completion_cycle_ : cycle;
    counters_.set(SmeshPerfEvent::StartupCycles, first_input_cycle_ - 1);
    counters_.set(SmeshPerfEvent::ExecutionCycles, end - first_input_cycle_ + 1);
    counters_.set(SmeshPerfEvent::DrainCycles, completed() ? cycle - end : 0);
  } else {
    counters_.set(SmeshPerfEvent::StartupCycles, cycle);
  }

  s_trace(rs_mem_perf_, "cycle=%" PRIu64 " input_fire=%u complete=%u execution=%" PRIu64
          " drain=%" PRIu64 "\n", cycle, unsigned(input_command_accepted),
          unsigned(workload_complete), counters_.value(SmeshPerfEvent::ExecutionCycles),
          counters_.value(SmeshPerfEvent::DrainCycles));
}

void RsMemPerfStats::report(const std::string& scenario) const {
  counters_.report(scenario);
  ::log("  execution_window   %s\n",
               !started() ? "not_started" : completed() ? "complete" : "incomplete");
}

} // namespace tb
} // namespace smesh
