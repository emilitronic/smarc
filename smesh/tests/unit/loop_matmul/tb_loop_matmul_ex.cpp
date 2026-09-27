// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_ex.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
// Focused test checks 16 commands, transpose cases, stalls, accumulator accumulation,
// and skipped request.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulEx.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

struct ExpectedCommand {
  smesh::SmeshFunct funct;
  std::uint64_t rs1;
  std::uint64_t rs2;
  std::uint16_t k;
  std::uint16_t j;
  std::uint16_t i;
  std::uint8_t loop_id;
};

class ExDriver : public Component {
  DECLARE_COMPONENT(ExDriver);

 public:
  ExDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, req_val);
  Input(bit, req_rdy);
  Output(smesh::LoopMatmulExReq, req_bits);
  Input(bit, cmd_val);
  Output(bit, cmd_rdy);
  Input(smesh::SmeshCmd, cmd_bits);
  Output(u16, ld_ka);
  Output(u16, ld_kb);
  Output(u16, ld_j);
  Output(u16, ld_i);
  Output(bit, lda_completed);
  Output(bit, ldb_completed);
  Output(bit, ldd_completed);
  Output(bit, ex_utilization_at_limit);
  Input(u16, k);
  Input(u16, j);
  Input(u16, i);
  Input(bit, idle);
  Input(u8, loop_id);

  void updateRequest();
  void updateRequestIndex();
  void updateDrive();
  void updateCycle();
  void observe();
  void reset();

  bool passed() const { return passed_ && observed_stall_ == 0x3u; }
  unsigned commandsAccepted() const { return command_pos_; }
  unsigned requestsAccepted() const { return static_cast<unsigned>(*request_pos_Q_); }

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, request_pos_Q_);
  Register(u8, request_pos_D_);

  std::array<smesh::LoopMatmulExReq, 4> requests_{{
      {1, 2, 2, 1, 1, 1, false, false, false, 0, 16, 0, 0, false},
      {1, 1, 2, 0, 0, 1, true, false, true, 0, 16, 0, 1, false},
      {2, 1, 1, 1, 0, 0, false, true, false, 0, 16, 0, 0, false},
      {1, 1, 1, 0, 0, 0, false, false, false, 0, 16, 0, 1, true},
  }};
  std::array<ExpectedCommand, 16> expected_{{
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(smesh::makeSpAddr(8), {4, 3}),
       smesh::packLocal(smesh::makeAccAddr(0), {4, 3}), 0, 0, 0, 0},
      {smesh::SmeshFunct::ComputeFlip,
       smesh::packLocal(smesh::makeSpAddr(0), {4, 4}),
       smesh::packLocal(0xffffffffu, {4, 4}), 0, 0, 0, 0},
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(0xffffffffu, {4, 3}),
       smesh::packLocal(smesh::makeAccAddr(4), {3, 3}), 0, 0, 1, 0},
      {smesh::SmeshFunct::ComputeStay,
       smesh::packLocal(smesh::makeSpAddr(8), {3, 4}),
       smesh::packLocal(0xffffffffu, {4, 4}), 0, 0, 1, 0},
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(smesh::makeSpAddr(12), {3, 3}),
       smesh::packLocal(smesh::makeAccAddr(0, true), {4, 3}), 1, 0, 0, 0},
      {smesh::SmeshFunct::ComputeFlip,
       smesh::packLocal(smesh::makeSpAddr(4), {4, 3}),
       smesh::packLocal(0xffffffffu, {4, 4}), 1, 0, 0, 0},
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(0xffffffffu, {3, 3}),
       smesh::packLocal(smesh::makeAccAddr(4, true), {3, 3}), 1, 0, 1, 0},
      {smesh::SmeshFunct::ComputeStay,
       smesh::packLocal(smesh::makeSpAddr(12), {3, 3}),
       smesh::packLocal(0xffffffffu, {4, 4}), 1, 0, 1, 0},
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(smesh::makeSpAddr(12), {4, 4}),
       smesh::packLocal(smesh::makeAccAddr(0, true), {4, 4}), 0, 0, 0, 1},
      {smesh::SmeshFunct::ComputeFlip,
       smesh::packLocal(smesh::makeSpAddr(0), {4, 4}),
       smesh::packLocal(0xffffffffu, {4, 4}), 0, 0, 0, 1},
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(0xffffffffu, {4, 4}),
       smesh::packLocal(smesh::makeAccAddr(4, true), {3, 4}), 0, 0, 1, 1},
      {smesh::SmeshFunct::ComputeStay,
       smesh::packLocal(smesh::makeSpAddr(4), {3, 4}),
       smesh::packLocal(0xffffffffu, {4, 4}), 0, 0, 1, 1},
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(smesh::makeSpAddr(8), {4, 4}),
       smesh::packLocal(smesh::makeAccAddr(0), {4, 4}), 0, 0, 0, 0},
      {smesh::SmeshFunct::ComputeFlip,
       smesh::packLocal(smesh::makeSpAddr(0), {4, 4}),
       smesh::packLocal(0xffffffffu, {4, 4}), 0, 0, 0, 0},
      {smesh::SmeshFunct::Preload,
       smesh::packLocal(smesh::makeSpAddr(12), {4, 3}),
       smesh::packLocal(smesh::makeAccAddr(4), {4, 3}), 0, 1, 0, 0},
      {smesh::SmeshFunct::ComputeFlip,
       smesh::packLocal(smesh::makeSpAddr(0), {4, 4}),
       smesh::packLocal(0xffffffffu, {4, 4}), 0, 1, 0, 0},
  }};

  unsigned command_pos_ = 0;
  unsigned observed_stall_ = 0;
  bool passed_ = true;
};

ExDriver::ExDriver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  request_pos_Q_ <= request_pos_D_;
  UPDATE(updateRequest).reads(request_pos_Q_).writes(req_val, req_bits);
  UPDATE(updateRequestIndex)
      .reads(request_pos_Q_, req_val, req_rdy).writes(request_pos_D_);
  UPDATE(updateDrive).reads(cycle_Q_)
      .writes(cmd_rdy, ld_ka, ld_kb, ld_j, ld_i, lda_completed,
              ldb_completed, ldd_completed)
      .writes(ex_utilization_at_limit);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
  UPDATE(observe)
      .reads(cycle_Q_, cmd_val, cmd_rdy, cmd_bits, k, j, i, idle)
      .reads(loop_id, req_rdy);
}

void ExDriver::updateRequest() {
  const auto pos = static_cast<unsigned>(*request_pos_Q_);
  req_val = bit(pos < requests_.size());
  req_bits = pos < requests_.size() ? requests_[pos] : smesh::LoopMatmulExReq{};
}

void ExDriver::updateRequestIndex() {
  if (req_val == 1 && req_rdy == 1) {
    request_pos_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*request_pos_Q_) + 1);
  }
}

void ExDriver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  ld_ka = bit(cycle == 4);
  ld_kb = 0;
  ld_j = bit(cycle >= 3 && cycle <= 5);
  ld_i = bit(cycle >= 2 && cycle <= 5);
  lda_completed = bit(cycle >= 6);
  ldb_completed = bit(cycle >= 6);
  ldd_completed = bit(cycle >= 1);
  ex_utilization_at_limit = bit(cycle == 5);
  cmd_rdy = bit(cycle >= 8);
}

void ExDriver::updateCycle() {
  cycle_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*cycle_Q_) + 1);
}

void ExDriver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if ((cycle == 1 || cycle == 2 || cycle == 4 || cycle == 5) && idle == 0) {
    passed_ &= cmd_val == 0;
    observed_stall_ |= 1u;
  }
  if ((cycle == 3 || cycle == 6 || cycle == 7) && idle == 0) {
    passed_ &= cmd_val == 1 && cmd_rdy == 0 &&
               static_cast<std::uint32_t>(cmd_bits->funct) ==
                   static_cast<std::uint32_t>(expected_[0].funct) &&
               static_cast<std::uint64_t>(cmd_bits->rs1) == expected_[0].rs1;
    observed_stall_ |= 2u;
  }
  if (cmd_val == 1 && cmd_rdy == 1) {
    if (command_pos_ >= expected_.size()) {
      passed_ = false;
    } else {
      const auto actual = *cmd_bits;
      const auto& expected = expected_[command_pos_];
      const bool match = static_cast<std::uint32_t>(actual.funct) ==
                             static_cast<std::uint32_t>(expected.funct) &&
                         static_cast<std::uint64_t>(actual.rs1) == expected.rs1 &&
                         static_cast<std::uint64_t>(actual.rs2) == expected.rs2 &&
                         k == expected.k && j == expected.j && i == expected.i &&
                         loop_id == expected.loop_id && idle == 0 && req_rdy == 0;
      if (!match) {
        std::printf("Ex command %u mismatch: funct=%u rs1=0x%llx rs2=0x%llx kji={%u,%u,%u}\n",
                    command_pos_, static_cast<unsigned>(actual.funct),
                    static_cast<unsigned long long>(actual.rs1),
                    static_cast<unsigned long long>(actual.rs2),
                    static_cast<unsigned>(*k), static_cast<unsigned>(*j),
                    static_cast<unsigned>(*i));
      }
      passed_ &= match;
    }
    ++command_pos_;
  }
}

void ExDriver::reset() {
  cycle_D_.reset(0);
  request_pos_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::LoopMatmulExReq{});
  cmd_rdy.reset(0);
  ld_ka.reset(0);
  ld_kb.reset(0);
  ld_j.reset(0);
  ld_i.reset(0);
  lda_completed.reset(0);
  ldb_completed.reset(0);
  ldd_completed.reset(0);
  ex_utilization_at_limit.reset(0);
  command_pos_ = 0;
  observed_stall_ = 0;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  ExDriver driver("Driver");
  smesh::LoopMatmulEx ex("Ex");
  ex.req_val << driver.req_val;
  ex.req_bits << driver.req_bits;
  driver.req_rdy << ex.req_rdy;
  ex.cmd_rdy << driver.cmd_rdy;
  driver.cmd_val << ex.cmd_val;
  driver.cmd_bits << ex.cmd_bits;
  ex.ld_ka << driver.ld_ka;
  ex.ld_kb << driver.ld_kb;
  ex.ld_j << driver.ld_j;
  ex.ld_i << driver.ld_i;
  ex.lda_completed << driver.lda_completed;
  ex.ldb_completed << driver.ldb_completed;
  ex.ldd_completed << driver.ldd_completed;
  ex.ex_utilization_at_limit << driver.ex_utilization_at_limit;
  driver.k << ex.k;
  driver.j << ex.j;
  driver.i << ex.i;
  driver.idle << ex.idle;
  driver.loop_id << ex.loop_id;

  Clock clk;
  driver.clk << clk;
  ex.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  bool passed = *ex.idle == 1 && *ex.req_rdy == 1 && *ex.cmd_val == 0;
  for (int cycle = 0; cycle < 36; ++cycle) Sim::run();
  passed &= driver.passed() && driver.requestsAccepted() == 4 &&
            driver.commandsAccepted() == 16 && *ex.idle == 1;

  descore::flushLog();
  std::printf("[LOOP_MATMUL_EX] %s requests=%u commands=%u\n",
              passed ? "PASS" : "FAIL", driver.requestsAccepted(), driver.commandsAccepted());
  return passed ? 0 : 1;
}
