// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_ld_ab_arb.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
// Focused test checks stalled and invalid-command cases.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulLdABArb.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

struct Step {
  bool a_val = true;
  bool b_val = true;
  bool a_idle = false;
  bool b_idle = false;
  std::uint16_t a_k = 0;
  std::uint16_t b_k = 0;
  std::uint16_t b_j = 1;
  std::uint8_t a_loop = 0;
  std::uint8_t b_loop = 0;
  std::uint8_t head = 0;
  bool resadd = false;
  bool ready = true;
  bool select_a = true;
};

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, a_val);
  Input(bit, a_rdy);
  Output(smesh::SmeshCmd, a_bits);
  Output(bit, a_idle);
  Output(u16, a_k);
  Output(u16, a_i);
  Output(u8, a_loop_id);
  Output(bit, b_val);
  Input(bit, b_rdy);
  Output(smesh::SmeshCmd, b_bits);
  Output(bit, b_idle);
  Output(u16, b_k);
  Output(u16, b_j);
  Output(u8, b_loop_id);
  Output(u8, head_loop_id);
  Output(bit, is_resadd);
  Input(bit, out_val);
  Output(bit, out_rdy);
  Input(smesh::SmeshCmd, out_bits);

  void drive();
  void nextStep();
  void check();
  void reset();
  bool passed() const;

 private:
  Output(u8, step_Q_);
  Register(u8, step_D_);
  std::array<Step, 15> steps_{};
  std::array<bool, 15> seen_{};
  bool passed_ = true;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  steps_[0].b_j = 0;
  steps_[0].select_a = false;
  steps_[2].a_k = 2;
  steps_[2].b_k = 1;
  steps_[2].select_a = false;
  steps_[3].a_k = 1;
  steps_[3].b_k = 2;
  steps_[4].a_idle = true;
  steps_[4].select_a = false;
  steps_[5].b_idle = true;
  steps_[6].b_loop = 1;
  steps_[7].b_loop = 1;
  steps_[7].head = 1;
  steps_[7].select_a = false;
  steps_[8].resadd = true;
  steps_[9].resadd = true;
  steps_[9].a_idle = true;
  steps_[9].select_a = false;
  steps_[10].resadd = true;
  steps_[10].b_loop = 1;
  steps_[10].head = 1;
  steps_[10].select_a = false;
  steps_[11].ready = false;
  steps_[12].a_val = false;
  steps_[13].b_val = false;
  steps_[13].b_j = 0;
  steps_[13].select_a = false;
  steps_[14].a_val = false;
  steps_[14].b_val = false;

  step_Q_ <= step_D_;
  UPDATE(drive).reads(step_Q_)
      .writes(a_val, a_bits, a_idle, a_k, a_i, a_loop_id, b_val, b_bits)
      .writes(b_idle, b_k, b_j, b_loop_id, head_loop_id, is_resadd, out_rdy);
  UPDATE(nextStep).reads(step_Q_).writes(step_D_);
  UPDATE(check).reads(step_Q_, a_rdy, b_rdy, out_val, out_bits);
}

void Driver::drive() {
  const auto index = static_cast<unsigned>(*step_Q_);
  const auto& s = steps_[index < steps_.size() ? index : steps_.size() - 1];
  smesh::SmeshCmd a{};
  a.funct = static_cast<unsigned>(smesh::SmeshFunct::Mvin);
  a.rs1 = 0xa0;
  smesh::SmeshCmd b{};
  b.funct = static_cast<unsigned>(smesh::SmeshFunct::Mvin2);
  b.rs1 = 0xb0;
  a_val = bit(s.a_val);
  a_bits = a;
  a_idle = bit(s.a_idle);
  a_k = s.a_k;
  a_i = 0;
  a_loop_id = s.a_loop;
  b_val = bit(s.b_val);
  b_bits = b;
  b_idle = bit(s.b_idle);
  b_k = s.b_k;
  b_j = s.b_j;
  b_loop_id = s.b_loop;
  head_loop_id = s.head;
  is_resadd = bit(s.resadd);
  out_rdy = bit(s.ready);
}

void Driver::nextStep() {
  const auto index = static_cast<unsigned>(*step_Q_);
  if (index < steps_.size()) step_D_ = static_cast<std::uint8_t>(index + 1);
}

void Driver::check() {
  if (Sim::state == Sim::SimResetting) return;
  const auto index = static_cast<unsigned>(*step_Q_);
  if (index >= steps_.size()) return;
  const auto& s = steps_[index];
  const bool expected_val = s.select_a ? s.a_val : s.b_val;
  const bool expected_a_rdy = s.select_a && s.ready;
  const bool expected_b_rdy = !s.select_a && s.ready;
  const auto actual = *out_bits;
  const bool match = out_val == bit(expected_val) &&
                     a_rdy == bit(expected_a_rdy) &&
                     b_rdy == bit(expected_b_rdy) &&
                     static_cast<std::uint64_t>(actual.rs1) == (s.select_a ? 0xa0u : 0xb0u);
  if (!match)
    std::printf("LdAB step %u mismatch: val=%u rdy={%u,%u} rs1=0x%llx\n",
                index, static_cast<unsigned>(*out_val), static_cast<unsigned>(*a_rdy),
                static_cast<unsigned>(*b_rdy),
                static_cast<unsigned long long>(actual.rs1));
  passed_ &= match;
  seen_[index] = true;
}

void Driver::reset() {
  step_D_.reset(0);
  a_val.reset(0);
  a_bits.reset(smesh::SmeshCmd{});
  a_idle.reset(1);
  a_k.reset(0);
  a_i.reset(0);
  a_loop_id.reset(0);
  b_val.reset(0);
  b_bits.reset(smesh::SmeshCmd{});
  b_idle.reset(1);
  b_k.reset(0);
  b_j.reset(0);
  b_loop_id.reset(0);
  head_loop_id.reset(0);
  is_resadd.reset(0);
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
  smesh::LoopMatmulLdABArb arb("LdAB");
  arb.a_val << driver.a_val;
  arb.a_bits << driver.a_bits;
  arb.a_idle << driver.a_idle;
  arb.a_k << driver.a_k;
  arb.a_i << driver.a_i;
  arb.a_loop_id << driver.a_loop_id;
  driver.a_rdy << arb.a_rdy;
  arb.b_val << driver.b_val;
  arb.b_bits << driver.b_bits;
  arb.b_idle << driver.b_idle;
  arb.b_k << driver.b_k;
  arb.b_j << driver.b_j;
  arb.b_loop_id << driver.b_loop_id;
  driver.b_rdy << arb.b_rdy;
  arb.head_loop_id << driver.head_loop_id;
  arb.is_resadd << driver.is_resadd;
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
  for (int cycle = 0; cycle < 18; ++cycle) Sim::run();
  descore::flushLog();
  std::printf("[LOOP_MATMUL_LD_AB_ARB] %s\n", driver.passed() ? "PASS" : "FAIL");
  return driver.passed() ? 0 : 1;
}
