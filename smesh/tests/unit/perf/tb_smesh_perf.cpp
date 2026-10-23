// **********************************************************************
// smesh/tests/unit/perf/tb_smesh_perf.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#include "tb_rs_to_mem_perf.hpp"

#include <cstdio>

using smesh::tb::RsMemPerfMonitor;
using smesh::tb::SmeshPerfEvent;

// Check inclusive execution endpoints and the partition of all sampled cycles.
bool matches(const RsMemPerfMonitor& monitor, std::uint64_t startup,
             std::uint64_t execution, std::uint64_t drain, std::uint64_t commands) {
  const auto& counters = monitor.counters();
  return counters.value(SmeshPerfEvent::StartupCycles) == startup &&
      counters.value(SmeshPerfEvent::ExecutionCycles) == execution &&
      counters.value(SmeshPerfEvent::DrainCycles) == drain &&
      counters.value(SmeshPerfEvent::InputCommands) == commands &&
      counters.value(SmeshPerfEvent::SimulationCycles) == startup + execution + drain;
}

int main() {
  RsMemPerfMonitor monitor;
  monitor.sample(false, false);
  monitor.sample(false, false);
  monitor.sample(true, false);
  monitor.sample(true, false);
  monitor.sample(false, false);
  monitor.sample(false, false);
  monitor.sample(false, true);
  monitor.sample(false, true);
  monitor.sample(false, true);
  bool ok = monitor.started() && monitor.completed() && matches(monitor, 2, 5, 2, 2);

  // Activity resumes during drain, so the earlier completion was provisional.
  monitor.sample(false, false);
  ok &= !monitor.completed() && matches(monitor, 2, 8, 0, 2);
  monitor.sample(false, true);
  monitor.sample(false, true);
  ok &= monitor.completed() && matches(monitor, 2, 9, 1, 2);

  monitor.reset();
  ok &= !monitor.started() && !monitor.completed() && matches(monitor, 0, 0, 0, 0);
  monitor.sample(true, true);
  monitor.sample(false, true);
  ok &= monitor.completed() && matches(monitor, 0, 1, 1, 1);

  monitor.reset();
  monitor.sample(false, true);
  ok &= !monitor.started() && !monitor.completed() && matches(monitor, 1, 0, 0, 0);
  monitor.sample(true, false);
  monitor.sample(false, false);
  ok &= monitor.started() && !monitor.completed() && matches(monitor, 1, 2, 0, 1);

  smesh::tb::SmeshPerfCounters exact;
  const auto large = (std::uint64_t{1} << 40) + 3;
  exact.set(SmeshPerfEvent::InputCommands, large);
  exact.add(SmeshPerfEvent::InputCommands, 7);
  ok &= exact.value(SmeshPerfEvent::InputCommands) == large + 7;

  std::printf("[SMESH_PERF] %s cycle boundaries, resumed activity, reset, integer totals\n",
              ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
