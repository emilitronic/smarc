// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_cmd_queue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConvCmdQueue.hpp"

#include <array>
#include <cstdio>

namespace {

// Distinct payloads check that buffering preserves both command and metadata.
smesh::SmeshQueuedCmd command(unsigned id) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = 20 + id;
  value.cmd.rs1 = 0x1234567800000000ull + id;
  value.cmd.rs2 = 0xabcdef0100000000ull + id;
  value.rs_tag = id;
  value.rs_tag_valid = bit(id != 2);
  value.from_mmul_loop = bit(id == 2);
  value.from_conv_loop = bit(id == 3);
  return value;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::SmeshQueuedCmd, in_bits);
  Input(bit, in_rdy);
  Input(bit, head_val);
  Input(smesh::SmeshQueuedCmd, head_bits);
  Output(bit, head_rdy);
  Input(bit, busy);

  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const { return passed_ && observed_ == 0x1ffu; }

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  bool passed_ = true;
  unsigned observed_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(in_val, in_bits, head_rdy);
  UPDATE(updateCheck).reads(cycle_Q_, in_rdy, head_val, head_bits, busy);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

// Fill two entries, stall the third, replace a consumed head, then drain.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  in_val = bit(cycle <= 4);
  in_bits = cycle <= 4 ? command(cycle < 2 ? cycle + 1 : 3) : smesh::SmeshQueuedCmd{};
  head_rdy = bit(cycle == 4 || cycle == 6 || cycle == 7);
}

// These expected heads also check that enqueue does not bypass the register.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  constexpr std::array<unsigned, 9> ids{{0, 1, 1, 1, 1, 2, 2, 3, 0}};
  constexpr std::array<unsigned, 9> ready{{1, 1, 0, 0, 1, 0, 1, 1, 1}};
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= ids.size()) return;
  bool match = head_val == bit(ids[cycle] != 0) &&
               busy == bit(ids[cycle] != 0) && in_rdy == ready[cycle];
  const auto actual = *head_bits;
  const auto expected = ids[cycle] != 0 ? command(ids[cycle]) : smesh::SmeshQueuedCmd{};
  match &= actual.cmd.funct == expected.cmd.funct &&
           actual.cmd.rs1 == expected.cmd.rs1 && actual.cmd.rs2 == expected.cmd.rs2 &&
           actual.rs_tag == expected.rs_tag && actual.rs_tag_valid == expected.rs_tag_valid &&
           actual.from_mmul_loop == expected.from_mmul_loop &&
           actual.from_conv_loop == expected.from_conv_loop;
  if (!match)
    std::printf("LoopConv queue cycle=%u ready=%u valid=%u tag=%u expected_tag=%u\n",
                cycle, static_cast<unsigned>(*in_rdy), static_cast<unsigned>(*head_val),
                unsigned(actual.rs_tag), ids[cycle]);
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
  head_rdy.reset(0);
  passed_ = true;
  observed_ = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LoopConvCmdQueue loop("CmdQueue");
  Driver driver("Driver");
  loop.cmd_val << driver.in_val;
  loop.cmd_bits << driver.in_bits;
  driver.in_rdy << loop.cmd_rdy;
  driver.head_val << loop.head_val;
  driver.head_bits << loop.head_bits;
  loop.head_rdy << driver.head_rdy;
  driver.busy << loop.head_val;

  Clock clk;
  loop.clk << clk;
  driver.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 9; ++cycle) Sim::run();

  const bool passed = driver.passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_CMD_QUEUE] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
