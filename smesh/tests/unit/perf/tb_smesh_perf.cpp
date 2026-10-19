// **********************************************************************
// smesh/tests/unit/perf/tb_smesh_perf.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#include "tb_rs_to_mem_perf.hpp"

#include <cstdio>

using smesh::tb::RsMemPerfObserver;
using smesh::tb::SmeshPerfEvent;

// Check inclusive execution endpoints and the partition of all sampled cycles.
bool matches(const RsMemPerfObserver& observer, std::uint64_t startup,
             std::uint64_t execution, std::uint64_t drain, std::uint64_t commands) {
  const auto& counters = observer.counters();
  return counters.value(SmeshPerfEvent::StartupCycles) == startup &&
      counters.value(SmeshPerfEvent::ExecutionCycles) == execution &&
      counters.value(SmeshPerfEvent::DrainCycles) == drain &&
      counters.value(SmeshPerfEvent::InputCommands) == commands &&
      counters.value(SmeshPerfEvent::SimulationCycles) == startup + execution + drain;
}

int main() {
  RsMemPerfObserver observer;
  observer.sample(false, false);
  observer.sample(false, false);
  observer.sample(true, false);
  observer.sample(true, false);
  observer.sample(false, false);
  observer.sample(false, false);
  observer.sample(false, true);
  observer.sample(false, true);
  observer.sample(false, true);
  bool ok = observer.started() && observer.completed() && matches(observer, 2, 5, 2, 2);

  // Activity resumes during drain, so the earlier completion was provisional.
  observer.sample(false, false);
  ok &= !observer.completed() && matches(observer, 2, 8, 0, 2);
  observer.sample(false, true);
  observer.sample(false, true);
  ok &= observer.completed() && matches(observer, 2, 9, 1, 2);

  observer.reset();
  ok &= !observer.started() && !observer.completed() && matches(observer, 0, 0, 0, 0);
  observer.sample(true, true);
  observer.sample(false, true);
  ok &= observer.completed() && matches(observer, 0, 1, 1, 1);

  observer.reset();
  observer.sample(false, true);
  ok &= !observer.started() && !observer.completed() && matches(observer, 1, 0, 0, 0);
  observer.sample(true, false);
  observer.sample(false, false);
  ok &= observer.started() && !observer.completed() && matches(observer, 1, 2, 0, 1);

  smesh::tb::SmeshPerfCounters exact;
  const auto large = (std::uint64_t{1} << 40) + 3;
  exact.set(SmeshPerfEvent::InputCommands, large);
  exact.add(SmeshPerfEvent::InputCommands, 7);
  ok &= exact.value(SmeshPerfEvent::InputCommands) == large + 7;

  std::printf("[SMESH_PERF] %s cycle boundaries, resumed activity, reset, integer totals\n",
              ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
