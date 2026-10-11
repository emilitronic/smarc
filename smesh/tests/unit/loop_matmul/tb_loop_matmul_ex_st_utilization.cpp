// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_ex_st_utilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulExUtilization.hpp"
#include "LoopMatmulStUtilization.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

class UtilDriver : public Component {
  DECLARE_COMPONENT(UtilDriver);

 public:
  UtilDriver(std::string name, unsigned capacity, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, first_cmd_fire);
  Output(bit, second_cmd_fire);
  Output(bit, any_cmd_fire);
  Output(u8, completed);
  Input(u16, outstanding);
  Input(bit, at_limit);

  void updateDrive();
  void updateCheck();
  void updateNextCycle();
  void reset();
  bool passed() const {
    return passed_ && std::all_of(observed_.begin(), observed_.end(), [](bool seen) { return seen; });
  }

 private:
  const unsigned capacity_;
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  bool passed_ = true;
  std::vector<bool> observed_;
};

UtilDriver::UtilDriver(std::string /*name*/, unsigned capacity, IMPL_CTOR)
    : capacity_(capacity), observed_(capacity + 6, false) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(first_cmd_fire, second_cmd_fire, any_cmd_fire, completed);
  UPDATE(updateCheck).reads(cycle_Q_, outstanding, at_limit);
  UPDATE(updateNextCycle).reads(cycle_Q_).writes(cycle_D_);
}

// Fill to capacity, hold, complete one, replace one, then drain all entries.
void UtilDriver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  first_cmd_fire = bit(cycle < capacity_);
  second_cmd_fire = bit(cycle == capacity_ + 3);
  any_cmd_fire = bit(cycle < capacity_ || cycle == capacity_ + 3);
  completed = cycle == capacity_ + 2 || cycle == capacity_ + 3 ? 1 :
              cycle == capacity_ + 4 ? capacity_ - 1 : 0;
}

// Check the current registered count before this cycle's transfers take effect.
void UtilDriver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= capacity_ + 6) return;
  const unsigned expected = cycle <= capacity_ ? cycle :
                            cycle <= capacity_ + 2 ? capacity_ :
                            cycle <= capacity_ + 4 ? capacity_ - 1 : 0;
  const bool match = outstanding == expected && at_limit == bit(expected >= capacity_);
  if (!match)
    std::printf("Utilization capacity=%u cycle=%u count=%u limit=%u expected=%u\n",
                capacity_, cycle, static_cast<unsigned>(*outstanding),
                static_cast<unsigned>(*at_limit), expected);
  passed_ &= match;
  observed_[cycle] = true;
}

void UtilDriver::updateNextCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void UtilDriver::reset() {
  cycle_D_.reset(0);
  first_cmd_fire.reset(0);
  second_cmd_fire.reset(0);
  any_cmd_fire.reset(0);
  completed.reset(0);
  passed_ = true;
  std::fill(observed_.begin(), observed_.end(), false);
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  UtilDriver ex_driver("ExDriver", smesh::kDefaultConfig.rs_execute_entries);
  UtilDriver st_driver("StDriver", smesh::kDefaultConfig.rs_store_entries);
  smesh::LoopMatmulExUtilization ex("ExUtilization");
  smesh::LoopMatmulStUtilization st("StUtilization");
  // Execute has one source; combine the driver's two issue phases.
  ex.ex_cmd_fire << ex_driver.any_cmd_fire;
  ex.ex_completed << ex_driver.completed;
  ex_driver.outstanding << ex.outstanding;
  ex_driver.at_limit << ex.ex_utilization_at_limit;
  st.st_c_cmd_fire << st_driver.first_cmd_fire;
  st.st_c_spad_cmd_fire << st_driver.second_cmd_fire;
  st.st_completed << st_driver.completed;
  st_driver.outstanding << st.outstanding;
  st_driver.at_limit << st.st_utilization_at_limit;

  Clock clk;
  ex_driver.clk << clk;
  st_driver.clk << clk;
  ex.clk << clk;
  st.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  const auto cycles = std::max(smesh::kDefaultConfig.rs_execute_entries,
                               smesh::kDefaultConfig.rs_store_entries) + 6;
  for (std::size_t cycle = 0; cycle < cycles; ++cycle) Sim::run();

  const bool passed = ex_driver.passed() && st_driver.passed();
  descore::flushLog();
  std::printf("[LOOP_MATMUL_EX_ST_UTILIZATION] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
