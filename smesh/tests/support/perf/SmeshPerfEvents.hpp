// **********************************************************************
// smesh/tests/support/perf/SmeshPerfEvents.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
#pragma once

#include <array>
#include <cstddef>

namespace smesh {
namespace tb {

// Named measurements shared by test monitors. Add events here as
// their observation conditions are implemented and checked.
enum class SmeshPerfEvent : std::size_t {
  SimulationCycles,
  StartupCycles,
  ExecutionCycles,
  DrainCycles,
  InputCommands,
  Count
};

constexpr std::size_t kSmeshPerfEventCount = static_cast<std::size_t>(SmeshPerfEvent::Count);
constexpr std::array<const char*, kSmeshPerfEventCount> kSmeshPerfEventNames{{
    "simulation_cycles", "startup_cycles", "execution_cycles", "drain_cycles", "input_commands"
}};

} // namespace tb
} // namespace smesh
