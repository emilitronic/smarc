// **********************************************************************
// smesh/tests/support/perf/SmeshPerfCounters.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#include "SmeshPerfCounters.hpp"

#include <descore.hpp>

#include <cinttypes>

namespace smesh {
namespace tb {

void SmeshPerfCounters::report(const std::string& scenario) const {
  ::log("[SMESH_PERF] %s\n", scenario.c_str());
  for (std::size_t i = 0; i < values_.size(); ++i) {
    ::log("  %-18s %" PRIu64 "\n", kSmeshPerfEventNames[i], values_[i]);
  }
}

} // namespace tb
} // namespace smesh
