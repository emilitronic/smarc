// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_cmd_arb.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulCmdArb.hpp"

#include <array>
#include <cstdio>
#include <cstdint>

namespace {

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, st_c_val);
  Input(bit, st_c_rdy);
  Output(smesh::SmeshCmd, st_c_bits);
  Output(bit, ex_val);
  Input(bit, ex_rdy);
  Output(smesh::SmeshCmd, ex_bits);
  Output(bit, ld_d_val);
  Input(bit, ld_d_rdy);
  Output(smesh::SmeshCmd, ld_d_bits);
  Output(bit, ld_ab_val);
  Input(bit, ld_ab_rdy);
  Output(smesh::SmeshCmd, ld_ab_bits);
  Output(bit, st_c_spad_val);
  Input(bit, st_c_spad_rdy);
  Output(smesh::SmeshCmd, st_c_spad_bits);
  Input(bit, out_val);
  Output(bit, out_rdy);
  Input(smesh::SmeshCmd, out_bits);

  void drive();
  void advance();
  void check();
  void reset();
  bool passed() const;

 private:
  Output(u8, step_Q_);
  Register(u8, step_D_);
  std::array<bool, 64> seen_{};
  bool passed_ = true;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  step_Q_ <= step_D_;
  UPDATE(drive).reads(step_Q_)
      .writes(st_c_val, st_c_bits, ex_val, ex_bits, ld_d_val, ld_d_bits,
              ld_ab_val, ld_ab_bits)
      .writes(st_c_spad_val, st_c_spad_bits, out_rdy);
  UPDATE(advance).reads(step_Q_).writes(step_D_);
  UPDATE(check)
      .reads(step_Q_, st_c_rdy, ex_rdy, ld_d_rdy, ld_ab_rdy,
             st_c_spad_rdy, out_val, out_bits);
}

void Driver::drive() {
  const auto step = static_cast<unsigned>(*step_Q_);
  const auto mask = step < 64 ? step / 2 : 0;
  st_c_val = bit((mask & 1u) != 0);
  ex_val = bit((mask & 2u) != 0);
  ld_d_val = bit((mask & 4u) != 0);
  ld_ab_val = bit((mask & 8u) != 0);
  st_c_spad_val = bit((mask & 16u) != 0);
  out_rdy = bit((step & 1u) != 0);

  smesh::SmeshCmd command{};
  command.rs1 = 0x10;
  st_c_bits = command;
  command.rs1 = 0x20;
  ex_bits = command;
  command.rs1 = 0x30;
  ld_d_bits = command;
  command.rs1 = 0x40;
  ld_ab_bits = command;
  command.rs1 = 0x50;
  st_c_spad_bits = command;
}

void Driver::advance() {
  const auto step = static_cast<unsigned>(*step_Q_);
  if (step < 64) step_D_ = static_cast<std::uint8_t>(step + 1);
}

void Driver::check() {
  if (Sim::state == Sim::SimResetting) return;
  const auto step = static_cast<unsigned>(*step_Q_);
  if (step >= 64) return;

  const auto mask = step / 2;
  const bool ready = (step & 1u) != 0;
  int selected = -1;
  for (int source = 0; source < 5; ++source) {
    if ((mask & (1u << source)) != 0) {
      selected = source;
      break;
    }
  }
  const std::array<unsigned, 5> readies{{
      static_cast<unsigned>(*st_c_rdy), static_cast<unsigned>(*ex_rdy),
      static_cast<unsigned>(*ld_d_rdy), static_cast<unsigned>(*ld_ab_rdy),
      static_cast<unsigned>(*st_c_spad_rdy),
  }};
  bool match = out_val == bit(selected >= 0);
  for (int source = 0; source < 5; ++source)
    match &= readies[source] == static_cast<unsigned>(ready && selected == source);
  const auto actual = *out_bits;
  match &= static_cast<std::uint64_t>(actual.rs1) ==
           (selected < 0 ? 0u : static_cast<unsigned>((selected + 1) * 0x10));
  if (!match)
    std::printf("CmdArb step=%u mask=0x%x ready=%u selected=%d got_val=%u got_rs1=0x%llx\n",
                step, mask, static_cast<unsigned>(ready), selected,
                static_cast<unsigned>(*out_val),
                static_cast<unsigned long long>(actual.rs1));
  passed_ &= match;
  seen_[step] = true;
}

void Driver::reset() {
  step_D_.reset(0);
  st_c_val.reset(0);
  st_c_bits.reset(smesh::SmeshCmd{});
  ex_val.reset(0);
  ex_bits.reset(smesh::SmeshCmd{});
  ld_d_val.reset(0);
  ld_d_bits.reset(smesh::SmeshCmd{});
  ld_ab_val.reset(0);
  ld_ab_bits.reset(smesh::SmeshCmd{});
  st_c_spad_val.reset(0);
  st_c_spad_bits.reset(smesh::SmeshCmd{});
  out_rdy.reset(0);
  seen_.fill(false);
  passed_ = true;
}

bool Driver::passed() const {
  if (!passed_) return false;
  for (const auto seen : seen_) if (!seen) return false;
  return true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::LoopMatmulCmdArb arb("CmdArb");
  arb.st_c_val << driver.st_c_val;
  arb.st_c_bits << driver.st_c_bits;
  driver.st_c_rdy << arb.st_c_rdy;
  arb.ex_val << driver.ex_val;
  arb.ex_bits << driver.ex_bits;
  driver.ex_rdy << arb.ex_rdy;
  arb.ld_d_val << driver.ld_d_val;
  arb.ld_d_bits << driver.ld_d_bits;
  driver.ld_d_rdy << arb.ld_d_rdy;
  arb.ld_ab_val << driver.ld_ab_val;
  arb.ld_ab_bits << driver.ld_ab_bits;
  driver.ld_ab_rdy << arb.ld_ab_rdy;
  arb.st_c_spad_val << driver.st_c_spad_val;
  arb.st_c_spad_bits << driver.st_c_spad_bits;
  driver.st_c_spad_rdy << arb.st_c_spad_rdy;
  driver.out_val << arb.out_val;
  driver.out_bits << arb.out_bits;
  arb.out_rdy << driver.out_rdy;

  Clock clk;
  driver.clk << clk;
  arb.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 66; ++cycle) Sim::run();
  descore::flushLog();
  std::printf("[LOOP_MATMUL_CMD_ARB] %s\n", driver.passed() ? "PASS" : "FAIL");
  return driver.passed() ? 0 : 1;
}
