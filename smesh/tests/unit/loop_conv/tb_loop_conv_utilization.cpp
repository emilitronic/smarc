// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_utilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 12 2026

#include "LoopConvUtilization.hpp"
#include <descore/Parameter.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

namespace {
using Counts = std::array<unsigned, 3>;
struct Step {
  Counts issued{};
  Counts completed{};
  Counts expected{}; // Registered counts visible BEFORE this step's edge.
};
constexpr Counts limits{{smesh::kDefaultConfig.rs_load_entries,
                         smesh::kDefaultConfig.rs_execute_entries,
                         smesh::kDefaultConfig.rs_store_entries}};

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string /*name*/, COMPONENT_CTOR) {
    Counts count{};
    auto append = [&](Counts issued, Counts completed) {
      steps_.push_back({issued, completed, count});
      for (unsigned g = 0; g < 3; ++g) count[g] = count[g] + issued[g] - completed[g];
    };
    append({}, {});
    // Fill each queue class independently and hold all three at their limits.
    for (unsigned g = 0; g < 3; ++g)
      for (unsigned n = 0; n < limits[g]; ++n) {
        Counts issued{}; issued[g] = 1; append(issued, {});
      }
    append({}, {}); append({}, {});
    append({}, {1, 1, 1}); // Credits lower registered counts on the next edge.
    // One accepted command plus one completion leaves its count unchanged.
    for (unsigned g = 0; g < 3; ++g) {
      if (count[g] > 0) {
        Counts both{}; both[g] = 1; append(both, both);
      }
    }
    while (count != Counts{}) {
      Counts completed{};
      for (unsigned g = 0; g < 3; ++g) completed[g] = std::min(count[g], 2u);
      append({}, completed);
    }
    append({}, {}); append({}, {});
    cycle_Q_ <= cycle_D_;
    UPDATE(updateDrive).reads(cycle_Q_).writes(ld_fire, ex_fire, st_fire, ld_completed, ex_completed, st_completed);
    UPDATE(updateCheck).reads(cycle_Q_, ld_count, ex_count, st_count, ld_limit, ex_limit, st_limit);
    UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
  }
  Clock(clk);
  Output(bit, ld_fire);
  Output(bit, ex_fire);
  Output(bit, st_fire);
  Output(u8, ld_completed);
  Output(u8, ex_completed);
  Output(u8, st_completed);
  Input(u16, ld_count);
  Input(u16, ex_count);
  Input(u16, st_count);
  Input(bit, ld_limit);
  Input(bit, ex_limit);
  Input(bit, st_limit);
  void updateDrive() {
    const auto n = unsigned(*cycle_Q_);
    const auto step = n < steps_.size() ? steps_[n] : Step{};
    ld_fire = step.issued[0]; ex_fire = step.issued[1]; st_fire = step.issued[2];
    ld_completed = step.completed[0]; ex_completed = step.completed[1]; st_completed = step.completed[2];
  }
  void updateCheck() {
    if (Sim::state == Sim::SimResetting) return;
    const auto n = unsigned(*cycle_Q_);
    if (n >= steps_.size()) return;
    const Counts actual{{unsigned(*ld_count), unsigned(*ex_count), unsigned(*st_count)}};
    const Counts full{{unsigned(*ld_limit), unsigned(*ex_limit), unsigned(*st_limit)}};
    bool match = actual == steps_[n].expected;
    for (unsigned g = 0; g < 3; ++g) match &= full[g] == unsigned(actual[g] >= limits[g]);
    if (!match) std::printf("Utilization cycle=%u counts={%u,%u,%u} expected={%u,%u,%u}\n", n,
        actual[0], actual[1], actual[2], steps_[n].expected[0], steps_[n].expected[1], steps_[n].expected[2]);
    passed_ &= match;
    ++observed_;
  }
  void updateCycle() { cycle_D_ = unsigned(*cycle_Q_) + 1; }
  void reset() {
    cycle_D_.reset(0);
    ld_fire.reset(0); ex_fire.reset(0); st_fire.reset(0);
    ld_completed.reset(0); ex_completed.reset(0); st_completed.reset(0);
    passed_ = true; observed_ = 0;
  }
  unsigned cycles() const { return steps_.size() + 2; }
  bool passed() const { return passed_ && observed_ == steps_.size(); }
 private:
  std::vector<Step> steps_;
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  bool passed_ = true;
  unsigned observed_ = 0;
};
} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConvUtilization utilization("Utilization");
  utilization.ld_cmd_fire << driver.ld_fire;
  utilization.ex_cmd_fire << driver.ex_fire;
  utilization.st_cmd_fire << driver.st_fire;
  utilization.ld_completed << driver.ld_completed;
  utilization.ex_completed << driver.ex_completed;
  utilization.st_completed << driver.st_completed;
  driver.ld_count << utilization.ld_outstanding;
  driver.ex_count << utilization.ex_outstanding;
  driver.st_count << utilization.st_outstanding;
  driver.ld_limit << utilization.ld_utilization_at_limit;
  driver.ex_limit << utilization.ex_utilization_at_limit;
  driver.st_limit << utilization.st_utilization_at_limit;
  Clock clk; driver.clk << clk; utilization.clk << clk; clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool passed = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < driver.cycles(); ++cycle) Sim::run();
    passed &= driver.passed();
    if (run == 0) {
      // Reset with an accepted command still outstanding, then replay.
      Sim::reset();
      for (unsigned cycle = 0; cycle < 3; ++cycle) Sim::run();
    }
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_UTILIZATION] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
