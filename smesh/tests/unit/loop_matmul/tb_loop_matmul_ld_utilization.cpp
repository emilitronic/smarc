// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_ld_utilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulLdUtilization.hpp"

#include <array>
#include <cstdio>

namespace {

class LdUtilDriver : public Component {
  DECLARE_COMPONENT(LdUtilDriver);

 public:
  LdUtilDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, lda_cmd_fire);
  Output(bit, ldb_cmd_fire);
  Output(bit, ldd_cmd_fire);
  Output(u8, ld_completed);
  Input(u16, outstanding);
  Input(bit, ld_utilization_at_limit);

  void updateDrive();
  void updateCheck();
  void updateNextCycle();
  void reset();

  bool passed() const { return passed_ && observed_ == 0xffu; }

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  bool passed_ = true;
  unsigned observed_ = 0;
};

LdUtilDriver::LdUtilDriver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(lda_cmd_fire, ldb_cmd_fire,
                                            ldd_cmd_fire, ld_completed);
  UPDATE(updateCheck).reads(cycle_Q_, outstanding, ld_utilization_at_limit);
  UPDATE(updateNextCycle).reads(cycle_Q_).writes(cycle_D_);
}

void LdUtilDriver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  lda_cmd_fire = bit(cycle == 0);
  ldb_cmd_fire = bit(cycle == 1);
  ldd_cmd_fire = bit(cycle == 4 || cycle == 5);
  ld_completed = cycle == 3 || cycle == 4 ? 1 : cycle == 6 ? 2 : 0;
}

void LdUtilDriver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  constexpr std::array<unsigned, 8> expected{{0, 1, 2, 2, 1, 1, 2, 0}};
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= expected.size()) return;
  const auto count = static_cast<unsigned>(*outstanding);
  const auto limit = static_cast<unsigned>(*ld_utilization_at_limit);
  const bool match = count == expected[cycle] &&
                     limit == (expected[cycle] >= smesh::kDefaultConfig.rs_load_entries);
  if (!match) {
    std::printf("Ld utilization cycle %u: count=%u limit=%u expected=%u\n",
                cycle, count, limit, expected[cycle]);
  }
  passed_ &= match;
  observed_ |= 1u << cycle;
}

void LdUtilDriver::updateNextCycle() {
  cycle_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*cycle_Q_) + 1);
}

void LdUtilDriver::reset() {
  cycle_D_.reset(0);
  lda_cmd_fire.reset(0);
  ldb_cmd_fire.reset(0);
  ldd_cmd_fire.reset(0);
  ld_completed.reset(0);
  passed_ = true;
  observed_ = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  LdUtilDriver driver("Driver");
  smesh::LoopMatmulLdUtilization utilization("LdUtilization");
  utilization.lda_cmd_fire << driver.lda_cmd_fire;
  utilization.ldb_cmd_fire << driver.ldb_cmd_fire;
  utilization.ldd_cmd_fire << driver.ldd_cmd_fire;
  utilization.ld_completed << driver.ld_completed;
  driver.outstanding << utilization.outstanding;
  driver.ld_utilization_at_limit << utilization.ld_utilization_at_limit;

  Clock clk;
  driver.clk << clk;
  utilization.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 8; ++cycle) Sim::run();
  const bool passed = driver.passed() && *utilization.outstanding == 0 &&
                      *utilization.ld_utilization_at_limit == 0;

  descore::flushLog();
  std::printf("[LOOP_MATMUL_LD_UTILIZATION] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
