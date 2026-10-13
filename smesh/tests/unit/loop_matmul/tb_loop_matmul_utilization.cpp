// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_utilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 13 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulUtilization.hpp"

#include <algorithm>
#include <cstdio>

namespace {

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Output(bit, lda_fire);
  Output(bit, ldb_fire);
  Output(bit, ldd_fire);
  Output(u8, ld_completed);
  Output(bit, ex_fire);
  Output(u8, ex_completed);
  Output(bit, st_c_fire);
  Output(bit, st_spad_fire);
  Output(u8, st_completed);
  Input(u16, ld_count);
  Input(u16, ex_count);
  Input(u16, st_count);
  Input(bit, ld_limit);
  Input(bit, ex_limit);
  Input(bit, st_limit);

  void updateDrive();
  void updateCheck();
  void updateNextCycle();
  void reset();
  bool passed() const { return passed_ && observed_ == cycles_; }

 private:
  static unsigned expectedCount(unsigned cycle, unsigned capacity);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  unsigned cycles_ = 0;
  bool passed_ = true;
  unsigned observed_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycles_ = std::max({smesh::kDefaultConfig.rs_load_entries,
                      smesh::kDefaultConfig.rs_execute_entries,
                      smesh::kDefaultConfig.rs_store_entries}) + 6;
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_)
      .writes(lda_fire, ldb_fire, ldd_fire, ld_completed)
      .writes(ex_fire, ex_completed, st_c_fire, st_spad_fire, st_completed);
  UPDATE(updateCheck).reads(cycle_Q_, ld_count, ex_count, st_count,
                            ld_limit, ex_limit, st_limit);
  UPDATE(updateNextCycle).reads(cycle_Q_).writes(cycle_D_);
}

unsigned Driver::expectedCount(unsigned cycle, unsigned capacity) {
  if (cycle <= capacity) return cycle;
  if (cycle <= capacity + 2) return capacity;
  if (cycle <= capacity + 4) return capacity - 1;
  return 0;
}

// Fill each class to its own limit, stall, replace a completed command, then drain.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto ld_capacity = static_cast<unsigned>(smesh::kDefaultConfig.rs_load_entries);
  const auto ex_capacity = static_cast<unsigned>(smesh::kDefaultConfig.rs_execute_entries);
  const auto st_capacity = static_cast<unsigned>(smesh::kDefaultConfig.rs_store_entries);

  lda_fire = bit(cycle < ld_capacity && cycle % 3 == 0);
  ldb_fire = bit(cycle < ld_capacity && cycle % 3 == 1);
  ldd_fire = bit((cycle < ld_capacity && cycle % 3 == 2) || cycle == ld_capacity + 3);
  ld_completed = cycle == ld_capacity + 2 || cycle == ld_capacity + 3 ? 1 :
                 cycle == ld_capacity + 4 ? ld_capacity - 1 : 0;

  ex_fire = bit(cycle < ex_capacity || cycle == ex_capacity + 3);
  ex_completed = cycle == ex_capacity + 2 || cycle == ex_capacity + 3 ? 1 :
                 cycle == ex_capacity + 4 ? ex_capacity - 1 : 0;

  st_c_fire = bit(cycle < st_capacity);
  st_spad_fire = bit(cycle == st_capacity + 3);
  st_completed = cycle == st_capacity + 2 || cycle == st_capacity + 3 ? 1 :
                 cycle == st_capacity + 4 ? st_capacity - 1 : 0;
}

// Observe registered counts before this cycle's issue and completion transfers.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= cycles_) return;
  const auto ld_expected = expectedCount(cycle, smesh::kDefaultConfig.rs_load_entries);
  const auto ex_expected = expectedCount(cycle, smesh::kDefaultConfig.rs_execute_entries);
  const auto st_expected = expectedCount(cycle, smesh::kDefaultConfig.rs_store_entries);
  const bool match = ld_count == ld_expected && ex_count == ex_expected && st_count == st_expected &&
      ld_limit == bit(ld_expected >= smesh::kDefaultConfig.rs_load_entries) &&
      ex_limit == bit(ex_expected >= smesh::kDefaultConfig.rs_execute_entries) &&
      st_limit == bit(st_expected >= smesh::kDefaultConfig.rs_store_entries);
  if (!match) {
    std::printf("LoopMatmul utilization cycle %u: counts={%u,%u,%u} expected={%u,%u,%u}\n",
                cycle, unsigned(*ld_count), unsigned(*ex_count), unsigned(*st_count),
                ld_expected, ex_expected, st_expected);
  }
  passed_ &= match;
  ++observed_;
}

void Driver::updateNextCycle() {
  cycle_D_ = static_cast<std::uint16_t>(static_cast<unsigned>(*cycle_Q_) + 1);
}

void Driver::reset() {
  cycle_D_.reset(0);
  lda_fire.reset(0); ldb_fire.reset(0); ldd_fire.reset(0); ld_completed.reset(0);
  ex_fire.reset(0); ex_completed.reset(0);
  st_c_fire.reset(0); st_spad_fire.reset(0); st_completed.reset(0);
  passed_ = true;
  observed_ = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::LoopMatmulUtilization utilization("Utilization");
  utilization.lda_cmd_fire << driver.lda_fire;
  utilization.ldb_cmd_fire << driver.ldb_fire;
  utilization.ldd_cmd_fire << driver.ldd_fire;
  utilization.ld_completed << driver.ld_completed;
  utilization.ex_cmd_fire << driver.ex_fire;
  utilization.ex_completed << driver.ex_completed;
  utilization.st_c_cmd_fire << driver.st_c_fire;
  utilization.st_c_spad_cmd_fire << driver.st_spad_fire;
  utilization.st_completed << driver.st_completed;
  driver.ld_count << utilization.ld_outstanding;
  driver.ex_count << utilization.ex_outstanding;
  driver.st_count << utilization.st_outstanding;
  driver.ld_limit << utilization.ld_utilization_at_limit;
  driver.ex_limit << utilization.ex_utilization_at_limit;
  driver.st_limit << utilization.st_utilization_at_limit;

  Clock clk;
  driver.clk << clk;
  utilization.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  const auto cycles = std::max({smesh::kDefaultConfig.rs_load_entries,
                                smesh::kDefaultConfig.rs_execute_entries,
                                smesh::kDefaultConfig.rs_store_entries}) + 6;
  for (std::size_t cycle = 0; cycle < cycles; ++cycle) Sim::run();

  const bool passed = driver.passed();
  descore::flushLog();
  std::printf("[LOOP_MATMUL_UTILIZATION] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
