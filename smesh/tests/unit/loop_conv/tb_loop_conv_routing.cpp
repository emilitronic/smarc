// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_routing.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConv.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>
#include <memory>

namespace {

smesh::SmeshQueuedCmd command(unsigned id, std::uint32_t funct) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = funct;
  value.cmd.rs1 = 0x1234567800000000ull + id;
  value.cmd.rs2 = 0xabcdef0100000000ull + id;
  value.rs_tag = id;
  value.rs_tag_valid = bit(id != 2);
  value.from_mmul_loop = bit(id == 2);
  value.from_conv_loop = bit(id == 3);
  return value;
}

// Check ordinary function codes on both sides of the convolution range.
smesh::SmeshQueuedCmd ordinary(unsigned id) {
  constexpr std::array<std::uint32_t, 3> functions{{14, 22, 23}};
  return command(id, functions[(id - 1) % functions.size()]);
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, smesh::SmeshFunct held_funct, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::SmeshQueuedCmd, in_bits);
  Input(bit, in_rdy);
  Input(bit, out_val);
  Input(smesh::SmeshQueuedCmd, out_bits);
  Output(bit, out_rdy);
  Input(bit, busy);

  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const { return passed_ && observed_ == 0xffffu; }

 private:
  const smesh::SmeshFunct held_funct_;
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  bool passed_ = true;
  unsigned observed_ = 0;
};

Driver::Driver(std::string /*name*/, smesh::SmeshFunct held_funct, IMPL_CTOR)
    : held_funct_(held_funct) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(in_val, in_bits, out_rdy);
  UPDATE(updateCheck).reads(cycle_Q_, in_rdy, out_val, out_bits, busy);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

// Drain ordinary commands, then put a loop command ahead of further traffic.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  in_val = bit(cycle <= 4 || cycle >= 9);
  if (cycle <= 4) {
    in_bits = ordinary(cycle < 2 ? cycle + 1 : 3);
  } else if (cycle == 9) {
    in_bits = command(4, static_cast<std::uint32_t>(held_funct_));
  } else if (cycle >= 10) {
    in_bits = ordinary(cycle == 10 ? 5 : 6);
  } else {
    in_bits = smesh::SmeshQueuedCmd{};
  }
  out_rdy = bit(cycle == 4 || cycle == 6 || cycle >= 7);
}

// Verify registered queue timing, stalled payloads, and loop-command retention.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= 16) return;
  constexpr std::array<unsigned, 10> ids{{0, 1, 1, 1, 1, 2, 2, 3, 0, 0}};
  constexpr std::array<unsigned, 10> ready{{1, 1, 0, 0, 1, 0, 1, 1, 1, 1}};
  const auto id = cycle < ids.size() ? ids[cycle] : 0;
  const auto expected_ready = cycle < ready.size() ? ready[cycle] : cycle == 10 ? 1u : 0u;
  const auto expected_busy = cycle < 10 ? unsigned(id != 0) : 1u;
  bool match = out_val == bit(id != 0) && in_rdy == expected_ready &&
               busy == expected_busy;
  const auto actual = *out_bits;
  const auto expected = id != 0 ? ordinary(id) : smesh::SmeshQueuedCmd{};
  match &= actual.cmd.funct == expected.cmd.funct &&
           actual.cmd.rs1 == expected.cmd.rs1 && actual.cmd.rs2 == expected.cmd.rs2 &&
           actual.rs_tag == expected.rs_tag && actual.rs_tag_valid == expected.rs_tag_valid &&
           actual.from_mmul_loop == expected.from_mmul_loop &&
           actual.from_conv_loop == expected.from_conv_loop;
  if (!match)
    std::printf("LoopConv routing held_funct=%u cycle=%u ready=%u valid=%u busy=%u tag=%u\n",
                static_cast<unsigned>(held_funct_), cycle, static_cast<unsigned>(*in_rdy),
                static_cast<unsigned>(*out_val), static_cast<unsigned>(*busy),
                unsigned(actual.rs_tag));
  passed_ &= match;
  observed_ |= 1u << cycle;
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void Driver::reset() {
  cycle_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
  passed_ = true;
  observed_ = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  // Independent instances exercise launch and each of the six config functions.
  constexpr std::array<smesh::SmeshFunct, 7> functions{{
    smesh::SmeshFunct::LoopConvWs,
    smesh::SmeshFunct::LoopConvWsConfig1,
    smesh::SmeshFunct::LoopConvWsConfig2,
    smesh::SmeshFunct::LoopConvWsConfig3,
    smesh::SmeshFunct::LoopConvWsConfig4,
    smesh::SmeshFunct::LoopConvWsConfig5,
    smesh::SmeshFunct::LoopConvWsConfig6,
  }};
  std::array<std::unique_ptr<smesh::LoopConv>, functions.size()> loops;
  std::array<std::unique_ptr<Driver>, functions.size()> drivers;
  Clock clk;
  for (std::size_t i = 0; i < functions.size(); ++i) {
    loops[i].reset(new smesh::LoopConv("LoopConv" + std::to_string(i)));
    drivers[i].reset(new Driver("Driver" + std::to_string(i), functions[i]));
    auto& loop = *loops[i];
    auto& driver = *drivers[i];
    loop.in_val << driver.in_val;
    loop.in_bits << driver.in_bits;
    driver.in_rdy << loop.in_rdy;
    driver.out_val << loop.out_val;
    driver.out_bits << loop.out_bits;
    loop.out_rdy << driver.out_rdy;
    driver.busy << loop.busy;
    loop.clk << clk;
    driver.clk << clk;
  }
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 16; ++cycle) Sim::run();

  bool passed = true;
  for (const auto& driver : drivers) passed &= driver->passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_ROUTING] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
