// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_stream.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Test on updated LoopMatmul (streaming iteration 2: now trakcs load, store, and execute utilization,
applies capacity backpressure, and releases completed loop slots for reuse).

This test runs three loops: two consecutive loops occupy both slots, and the third
reuses slot 0.  It also checks delayed completions and load-capacity backpressure.

*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmul.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>
#include <cstdint>

namespace {

// Delayed RS completions fill the small load capacity before issuing can resume.
using CompletionPipe = std::array<u8, 12>;

smesh::SmeshQueuedCmd command(smesh::SmeshFunct funct, std::uint64_t rs1,
                              std::uint64_t rs2) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = static_cast<std::uint32_t>(funct);
  value.cmd.rs1 = rs1;
  value.cmd.rs2 = rs2;
  return value;
}

smesh::SmeshCmd expected(smesh::SmeshFunct funct, std::uint64_t rs1,
                         std::uint64_t rs2) {
  smesh::SmeshCmd value{};
  value.funct = static_cast<std::uint32_t>(funct);
  value.rs1 = rs1;
  value.rs2 = rs2;
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
  Output(u8, ld_completed);
  Output(u8, st_completed);
  Output(u8, ex_completed);
  Input(bit, completed0);
  Input(bit, completed1);

  void drive();
  void advance();
  void driveCompletion();
  void advanceCompletion();
  void observe();
  void reset();
  bool passed() const;
  unsigned commands() const { return commands_; }
  unsigned completions() const { return completions_; }
  unsigned accepted() const { return static_cast<unsigned>(*program_Q_); }

 private:
  Output(u8, program_Q_);
  Register(u8, program_D_);
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(CompletionPipe, completion_pipe_Q_);
  Register(CompletionPipe, completion_pipe_D_);
  std::array<smesh::SmeshQueuedCmd, 18> program_{};
  std::array<smesh::SmeshCmd, 18> expected_{};
  std::array<bool, 18> seen_{};
  std::array<unsigned, 3> completion_order_{};
  unsigned commands_ = 0;
  unsigned completions_ = 0;
  std::array<unsigned, 3> first_load_cycles_{};
  unsigned first_loads_ = 0;
  bool passed_ = true;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  for (unsigned n = 0; n < 3; ++n) {
    const auto base = std::uint64_t{0x1000} + std::uint64_t{n} * 0x4000;
    const auto local_a = (n % 2) * 8u;
    const auto local_b = local_a + 4u;
    const auto local_acc = (n % 2) * 8u;
    const auto p = n * 6;
    program_[p + 0] = command(smesh::SmeshFunct::LoopWsBounds, 0,
                              (1ull << 32) | (1ull << 16) | 1ull);
    program_[p + 1] = command(smesh::SmeshFunct::LoopWsAddrsAb, base, base + 0x1000);
    program_[p + 2] = command(smesh::SmeshFunct::LoopWsAddrsDc,
                              base + 0x2000, base + 0x3000);
    program_[p + 3] = command(smesh::SmeshFunct::LoopWsStridesAb, 4, 4);
    program_[p + 4] = command(smesh::SmeshFunct::LoopWsStridesDc, 4, 4);
    program_[p + 5] = command(smesh::SmeshFunct::LoopWs, 0, 0);

    expected_[p + 0] = expected(smesh::SmeshFunct::Mvin3, base + 0x2000,
                                smesh::packLocal(smesh::makeAccAddr(local_acc), {4, 4}));
    expected_[p + 1] = expected(smesh::SmeshFunct::Mvin2, base + 0x1000,
                                smesh::packLocal(smesh::makeSpAddr(local_b), {4, 4}));
    expected_[p + 2] = expected(smesh::SmeshFunct::Mvin, base,
                                smesh::packLocal(smesh::makeSpAddr(local_a), {4, 4}));
    expected_[p + 3] = expected(smesh::SmeshFunct::Preload,
                                smesh::packLocal(smesh::makeSpAddr(local_b), {4, 4}),
                                smesh::packLocal(smesh::makeAccAddr(local_acc), {4, 4}));
    expected_[p + 4] = expected(smesh::SmeshFunct::ComputeFlip,
                                smesh::packLocal(smesh::makeSpAddr(local_a), {4, 4}),
                                smesh::packLocal(smesh::makeLocalAddr(0xffffffffu), {4, 4}));
    expected_[p + 5] = expected(smesh::SmeshFunct::Mvout, base + 0x3000,
                                smesh::packLocal(smesh::makeAccAddr(local_acc), {4, 4}));
  }

  program_Q_ <= program_D_;
  cycle_Q_ <= cycle_D_;
  completion_pipe_Q_ <= completion_pipe_D_;
  UPDATE(drive).reads(program_Q_, cycle_Q_).writes(in_val, in_bits, out_rdy);
  UPDATE(advance).reads(program_Q_, cycle_Q_, in_val, in_rdy)
      .writes(program_D_, cycle_D_);
  UPDATE(driveCompletion).reads(completion_pipe_Q_)
      .writes(ld_completed, st_completed, ex_completed);
  UPDATE(advanceCompletion).reads(out_val, out_rdy, out_bits, completion_pipe_Q_)
      .writes(completion_pipe_D_);
  UPDATE(observe).reads(cycle_Q_, out_val, out_rdy, out_bits, completed0, completed1);
}

void Driver::drive() {
  const auto pos = static_cast<unsigned>(*program_Q_);
  in_val = bit(pos < program_.size());
  in_bits = pos < program_.size() ? program_[pos] : smesh::SmeshQueuedCmd{};
  out_rdy = bit(static_cast<unsigned>(*cycle_Q_) % 4 != 0);
}

void Driver::advance() {
  if (in_val == 1 && in_rdy == 1)
    program_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*program_Q_) + 1);
  cycle_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*cycle_Q_) + 1);
}

void Driver::driveCompletion() {
  const auto pipe = *completion_pipe_Q_;
  ld_completed = bit(pipe.back() == 1);
  st_completed = bit(pipe.back() == 2);
  ex_completed = bit(pipe.back() == 3);
}

void Driver::advanceCompletion() {
  std::uint8_t kind = 0;
  if (out_val == 1 && out_rdy == 1) {
    const auto value = *out_bits;
    const auto funct = static_cast<smesh::SmeshFunct>(
        static_cast<std::uint32_t>(value.cmd.funct));
    if (funct == smesh::SmeshFunct::Mvin ||
        funct == smesh::SmeshFunct::Mvin2 ||
        funct == smesh::SmeshFunct::Mvin3) kind = 1;
    else if (funct == smesh::SmeshFunct::Mvout ||
             funct == smesh::SmeshFunct::StoreSpad) kind = 2;
    else if (funct == smesh::SmeshFunct::Preload ||
             funct == smesh::SmeshFunct::ComputeFlip ||
             funct == smesh::SmeshFunct::ComputeStay) kind = 3;
  }
  auto next = *completion_pipe_Q_;
  for (std::size_t n = next.size() - 1; n > 0; --n) next[n] = next[n - 1];
  next[0] = kind;
  completion_pipe_D_ = next;
}

void Driver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  if (completed0 == 1 || completed1 == 1) {
    if (completions_ < completion_order_.size())
      completion_order_[completions_] = completed1 == 1 ? 1u : 0u;
    else passed_ = false;
    ++completions_;
  }
  if (out_val == 0 || out_rdy == 0) return;
  const auto actual = *out_bits;
  const auto funct = static_cast<smesh::SmeshFunct>(
      static_cast<std::uint32_t>(actual.cmd.funct));
  if (first_loads_ < first_load_cycles_.size() &&
      (funct == smesh::SmeshFunct::Mvin ||
       funct == smesh::SmeshFunct::Mvin2 ||
       funct == smesh::SmeshFunct::Mvin3))
    first_load_cycles_[first_loads_++] = static_cast<unsigned>(*cycle_Q_);
  bool matched = false;
  for (unsigned n = 0; n < expected_.size(); ++n) {
    if (seen_[n]) continue;
    if (actual.cmd.funct == expected_[n].funct &&
        actual.cmd.rs1 == expected_[n].rs1 &&
        actual.cmd.rs2 == expected_[n].rs2 &&
        actual.from_mmul_loop == 1) {
      matched = true;
      seen_[n] = true;
      break;
    }
  }
  if (!matched)
    std::printf("unexpected command %u: funct=%u rs1=0x%llx rs2=0x%llx\n",
                commands_, static_cast<unsigned>(actual.cmd.funct),
                static_cast<unsigned long long>(actual.cmd.rs1),
                static_cast<unsigned long long>(actual.cmd.rs2));
  passed_ &= matched;
  ++commands_;
}

void Driver::reset() {
  program_D_.reset(0);
  cycle_D_.reset(0);
  completion_pipe_D_.reset({});
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
  ld_completed.reset(0);
  st_completed.reset(0);
  ex_completed.reset(0);
  seen_.fill(false);
  completion_order_.fill(0);
  commands_ = 0;
  completions_ = 0;
  first_load_cycles_.fill(0);
  first_loads_ = 0;
  passed_ = true;
}

bool Driver::passed() const {
  if (!passed_ || commands_ != expected_.size() || completions_ != 3 ||
      first_loads_ != 3 || first_load_cycles_[2] - first_load_cycles_[1] < 4 ||
      completion_order_ != std::array<unsigned, 3>{{0, 1, 0}}) return false;
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
  loop.ld_completed << driver.ld_completed;
  loop.st_completed << driver.st_completed;
  loop.ex_completed << driver.ex_completed;
  driver.completed0 << loop.completed0;
  driver.completed1 << loop.completed1;

  Clock clk;
  driver.clk << clk;
  loop.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 150; ++cycle) Sim::run();
  const bool slots_free = loop.loop0->configured == 0 &&
                          loop.loop1->configured == 0 && *loop.busy == 0 &&
                          *loop.head_loop_id == 1;
  const bool passed = driver.passed() && driver.accepted() == 18 && slots_free;
  descore::flushLog();
  std::printf("[LOOP_MATMUL_REUSE] %s commands=%u completions=%u accepted=%u free=%u\n",
              passed ? "PASS" : "FAIL", driver.commands(), driver.completions(),
              driver.accepted(), static_cast<unsigned>(slots_free));
  return passed ? 0 : 1;
}
