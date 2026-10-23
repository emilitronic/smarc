// **********************************************************************
// smesh/tests/support/perf/SmeshPerfCounters.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#pragma once

#include "SmeshPerfEvents.hpp"

#include <cstdint>
#include <string>

namespace smesh {
namespace tb {

// Simulation-only integer counters. Monitors decide which events occurred;
// this class stores exact totals and reports them through Descore logging.
class SmeshPerfCounters {
 public:
  void reset() { values_ = {}; }
  void add(SmeshPerfEvent event, std::uint64_t amount = 1) {
    values_.at(static_cast<std::size_t>(event)) += amount;
  }
  void set(SmeshPerfEvent event, std::uint64_t value) {
    values_.at(static_cast<std::size_t>(event)) = value;
  }
  std::uint64_t value(SmeshPerfEvent event) const {
    return values_.at(static_cast<std::size_t>(event));
  }
  void report(const std::string& scenario) const;

 private:
  std::array<std::uint64_t, kSmeshPerfEventCount> values_{};
};

} // namespace tb
} // namespace smesh
