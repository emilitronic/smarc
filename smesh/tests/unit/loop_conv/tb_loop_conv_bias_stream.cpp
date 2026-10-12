// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_bias_stream.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConv.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

smesh::SmeshQueuedCmd command(unsigned funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = funct;
  value.cmd.rs1 = rs1;
  value.cmd.rs2 = rs2;
  return value;
}

bool same(const smesh::SmeshQueuedCmd& a, const smesh::SmeshQueuedCmd& b) {
  return a.cmd.funct == b.cmd.funct && a.cmd.rs1 == b.cmd.rs1 && a.cmd.rs2 == b.cmd.rs2 &&
         a.rs_tag == b.rs_tag && a.rs_tag_valid == b.rs_tag_valid &&
         a.from_mmul_loop == b.from_mmul_loop && a.from_conv_loop == b.from_conv_loop;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::SmeshQueuedCmd, in_bits);
  Input(bit, in_rdy);
  Output(bit, out_rdy);
  Input(bit, out_val);
  Input(smesh::SmeshQueuedCmd, out_bits);
  Input(smesh::LoopConvSlot, loop0);
  Input(smesh::LoopConvSlot, loop1);
  Input(bit, busy);

  void updateDrive();
  void updatePosition();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const {
    const auto first = *loop0;
    const auto second = *loop1;
    return passed_ && seen_ == expected_.size() && *position_Q_ == program_.size() &&
           saw_stall_ && first.ld_bias_started == 1 && first.ld_bias_completed == 1 &&
           second.ld_bias_started == 1 && second.ld_bias_completed == 1 &&
           first.ld_input_started == 1 && first.ld_input_completed == 1 &&
           second.ld_input_started == 1 && second.ld_input_completed == 1 &&
           first.ld_weights_started == 1 && first.ld_weights_completed == 1 &&
           second.ld_weights_started == 1 && second.ld_weights_completed == 1 &&
           first.configured == 1 && second.configured == 1 && *busy == 1;
  }

 private:
  std::array<smesh::SmeshQueuedCmd, 12> program_{};
  std::array<smesh::SmeshQueuedCmd, 7> expected_{};
  Output(u8, position_Q_);
  Register(u8, position_D_);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  bool passed_ = true;
  unsigned seen_ = 0;
  bool stalled_ = false;
  bool saw_stall_ = false;
  smesh::SmeshQueuedCmd stalled_command_{};
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  // An ordinary command must pass through with its metadata intact.
  program_[0] = command(7, 0x1234, 0x5678);
  program_[0].rs_tag = 9;
  program_[0].rs_tag_valid = 1;
  program_[0].from_mmul_loop = 1;
  expected_[0] = program_[0];
  for (unsigned slot = 0; slot < 2; ++slot) {
    const auto p = 1 + slot * 5;
    program_[p] = command(17, 0, (1ull << 48) | 3);
    program_[p + 1] = command(19, (1ull << 48) | 1, 5);
    program_[p + 2] = command(20, 0, 0x80003000 + slot * 0x1000);
    program_[p + 3] = command(21, 0x80001000 + slot * 0x1000, 0);
    // Slot 1 requests zero bias filling, but its source address is nonzero.
    program_[p + 4] = command(15, slot, 0);
  }
  // This ordinary command must wait for loop retirement, which is not built yet.
  program_[11] = command(7, 0xbeef, 0xcafe);
  expected_[1] = command(0, 0x3f80000000050111ull, 0);
  expected_[2] = command(14, 0x80001000, 0x0004000380000000ull);
  expected_[3] = command(14, 0x80001000, 0x0001000380000004ull);
  expected_[4] = command(0, 0x3f80000000050111ull, 0);
  expected_[5] = command(14, 0, 0x0004000380000008ull);
  expected_[6] = command(14, 0, 0x000100038000000cull);
  for (std::size_t i = 1; i < expected_.size(); ++i) expected_[i].from_conv_loop = 1;

  position_Q_ <= position_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(position_Q_, cycle_Q_).writes(in_val, in_bits, out_rdy);
  UPDATE(updatePosition).reads(position_Q_, in_val, in_rdy).writes(position_D_);
  UPDATE(updateCheck).reads(out_val, out_bits, out_rdy, loop0, loop1);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

void Driver::updateDrive() {
  const auto position = static_cast<unsigned>(*position_Q_);
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  in_val = bit(position < program_.size());
  in_bits = position < program_.size() ? program_[position] : smesh::SmeshQueuedCmd{};
  out_rdy = bit(cycle % 4 != 2 && !(cycle >= 12 && cycle <= 16));
}

void Driver::updatePosition() {
  if (in_val == 1 && in_rdy == 1) position_D_ = static_cast<unsigned>(*position_Q_) + 1;
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  if (stalled_) passed_ &= out_val == 1 && same(*out_bits, stalled_command_);
  if (out_val == 0) return;
  const auto actual = *out_bits;
  if (out_rdy == 0) {
    stalled_ = true;
    stalled_command_ = actual;
    saw_stall_ = true;
    return;
  }
  stalled_ = false;
  const bool match = seen_ < expected_.size() && same(actual, expected_[seen_]);
  if (!match)
    std::printf("Bias stream command %u mismatch funct=%u rs1=0x%llx rs2=0x%llx\n",
                seen_, static_cast<unsigned>(actual.cmd.funct),
                static_cast<unsigned long long>(actual.cmd.rs1),
                static_cast<unsigned long long>(actual.cmd.rs2));
  passed_ &= match;
  if (actual.from_conv_loop == 1) {
    const auto first = *loop0;
    const auto second = *loop1;
    // The generating slot must remain incomplete until its pipeline drains.
    passed_ &= seen_ <= 3 ? first.ld_bias_completed == 0 : second.ld_bias_completed == 0;
  }
  ++seen_;
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void Driver::reset() {
  position_D_.reset(0);
  cycle_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
  passed_ = true;
  seen_ = 0;
  stalled_ = false;
  saw_stall_ = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConv loop("LoopConv");
  loop.in_val << driver.in_val;
  loop.in_bits << driver.in_bits;
  driver.in_rdy << loop.in_rdy;
  loop.out_rdy << driver.out_rdy;
  driver.out_val << loop.out_val;
  driver.out_bits << loop.out_bits;
  driver.loop0 << loop.loop0;
  driver.loop1 << loop.loop1;
  driver.busy << loop.busy;
  Clock clk;
  driver.clk << clk;
  loop.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 100; ++cycle) Sim::run();
  bool passed = driver.passed();
  Sim::reset();
  for (int cycle = 0; cycle < 100; ++cycle) Sim::run();
  passed &= driver.passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_BIAS_STREAM] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
