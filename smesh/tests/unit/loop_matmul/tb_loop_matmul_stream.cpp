// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_stream.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Verification for LoopMatmul streaming iteration 1 (launches each generator once
per configured slot, connects load ane execute progress, and records when each
generator finishes, no utilization limits and completed slot release implemented yet).

This verifies one-tile command order: LdD, LdB, LdA, PRELOAD, COMPUTE_FLIP, STORE.
It checks payloads and that a command stays stable under backpressure.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmul.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

smesh::SmeshQueuedCmd command(smesh::SmeshFunct funct, std::uint64_t rs1,
                              std::uint64_t rs2) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = static_cast<std::uint32_t>(funct);
  value.cmd.rs1 = rs1;
  value.cmd.rs2 = rs2;
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
  Input(bit, out_val);
  Input(smesh::SmeshQueuedCmd, out_bits);
  Output(bit, out_rdy);

  void drive();
  void advance();
  void observe();
  void reset();
  bool passed() const;
  unsigned sent() const { return sent_; }
  unsigned accepted() const { return static_cast<unsigned>(*program_Q_); }

 private:
  Output(u8, program_Q_);
  Register(u8, program_D_);
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  const std::array<smesh::SmeshQueuedCmd, 6> program_{{
      command(smesh::SmeshFunct::LoopWsBounds, 0, (1ull << 32) | (1ull << 16) | 1ull),
      command(smesh::SmeshFunct::LoopWsAddrsAb, 0x1000, 0x2000),
      command(smesh::SmeshFunct::LoopWsAddrsDc, 0x3000, 0x4000),
      command(smesh::SmeshFunct::LoopWsStridesAb, 4, 4),
      command(smesh::SmeshFunct::LoopWsStridesDc, 4, 4),
      command(smesh::SmeshFunct::LoopWs, 0, 0),
  }};
  const std::array<smesh::SmeshCmd, 6> expected_{{
      {static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin3), 0x3000,
       smesh::packLocal(smesh::makeAccAddr(0), {4, 4})},
      {static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin2), 0x2000,
       smesh::packLocal(smesh::makeSpAddr(4), {4, 4})},
      {static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin), 0x1000,
       smesh::packLocal(smesh::makeSpAddr(0), {4, 4})},
      {static_cast<std::uint32_t>(smesh::SmeshFunct::Preload),
       smesh::packLocal(smesh::makeSpAddr(4), {4, 4}),
       smesh::packLocal(smesh::makeAccAddr(0), {4, 4})},
      {static_cast<std::uint32_t>(smesh::SmeshFunct::ComputeFlip),
       smesh::packLocal(smesh::makeSpAddr(0), {4, 4}),
       smesh::packLocal(smesh::makeLocalAddr(0xffffffffu), {4, 4})},
      {static_cast<std::uint32_t>(smesh::SmeshFunct::Mvout), 0x4000,
       smesh::packLocal(smesh::makeAccAddr(0), {4, 4})},
  }};
  std::array<bool, 6> seen_{};
  unsigned sent_ = 0;
  bool saw_stall_ = false;
  bool waiting_ = false;
  smesh::SmeshQueuedCmd held_{};
  bool passed_ = true;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  program_Q_ <= program_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(drive).reads(program_Q_, cycle_Q_).writes(in_val, in_bits, out_rdy);
  UPDATE(advance)
      .reads(program_Q_, cycle_Q_, in_val, in_rdy)
      .writes(program_D_, cycle_D_);
  UPDATE(observe).reads(out_val, out_bits, out_rdy);
}

void Driver::drive() {
  const auto pos = static_cast<unsigned>(*program_Q_);
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  in_val = bit(pos < program_.size());
  in_bits = pos < program_.size() ? program_[pos] : smesh::SmeshQueuedCmd{};
  out_rdy = bit(cycle % 3 != 0);
}

void Driver::advance() {
  if (in_val == 1 && in_rdy == 1)
    program_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*program_Q_) + 1);
  cycle_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*cycle_Q_) + 1);
}

void Driver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  if (waiting_) {
    const auto actual = *out_bits;
    passed_ &= out_val == 1 &&
               actual.cmd.funct == held_.cmd.funct &&
               actual.cmd.rs1 == held_.cmd.rs1 &&
               actual.cmd.rs2 == held_.cmd.rs2;
  }
  if (out_val == 0) return;
  if (out_rdy == 0) {
    saw_stall_ = true;
    waiting_ = true;
    held_ = *out_bits;
    return;
  }
  waiting_ = false;
  const auto actual = *out_bits;
  bool matched = false;
  if (sent_ < expected_.size()) {
    const auto& expected = expected_[sent_];
    matched = static_cast<std::uint32_t>(actual.cmd.funct) ==
                  static_cast<std::uint32_t>(expected.funct) &&
              static_cast<std::uint64_t>(actual.cmd.rs1) ==
                  static_cast<std::uint64_t>(expected.rs1) &&
              static_cast<std::uint64_t>(actual.cmd.rs2) ==
                  static_cast<std::uint64_t>(expected.rs2) &&
              actual.from_mmul_loop == 1;
    if (matched) seen_[sent_] = true;
  }
  if (!matched) {
    std::printf("unexpected cmd %u: funct=%u rs1=0x%llx rs2=0x%llx\n", sent_,
                static_cast<unsigned>(actual.cmd.funct),
                static_cast<unsigned long long>(actual.cmd.rs1),
                static_cast<unsigned long long>(actual.cmd.rs2));
  }
  passed_ &= matched;
  ++sent_;
}

void Driver::reset() {
  program_D_.reset(0);
  cycle_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
  seen_.fill(false);
  sent_ = 0;
  saw_stall_ = false;
  waiting_ = false;
  held_ = smesh::SmeshQueuedCmd{};
  passed_ = true;
}

bool Driver::passed() const {
  if (!passed_ || !saw_stall_ || sent_ != expected_.size()) return false;
  for (const auto seen : seen_) if (!seen) return false;
  return true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::LoopMatmul loop("LoopMatmul");
  loop.in_val << driver.in_val;
  loop.in_bits << driver.in_bits;
  driver.in_rdy << loop.in_rdy;
  loop.out_rdy << driver.out_rdy;
  driver.out_val << loop.out_val;
  driver.out_bits << loop.out_bits;

  Clock clk;
  driver.clk << clk;
  loop.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 80; ++cycle) Sim::run();
  const auto slot = *loop.loop0;
  const bool complete = slot.lda_completed == 1 && slot.ldb_completed == 1 &&
                        slot.ldd_completed == 1 && slot.ex_completed == 1 &&
                        slot.st_completed == 1;
  const bool passed = driver.passed() && driver.accepted() == 6 && complete;
  descore::flushLog();
  std::printf("[LOOP_MATMUL_STREAM] %s commands=%u completed=%u%u%u%u%u\n",
              passed ? "PASS" : "FAIL", driver.sent(),
              static_cast<unsigned>(slot.lda_completed == 1),
              static_cast<unsigned>(slot.ldb_completed == 1),
              static_cast<unsigned>(slot.ldd_completed == 1),
              static_cast<unsigned>(slot.ex_completed == 1),
              static_cast<unsigned>(slot.st_completed == 1));
  return passed ? 0 : 1;
}
