// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_st_c.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulStC.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

struct ExpectedCommand {
  smesh::SmeshFunct funct;
  std::uint64_t rs1;
  std::uint64_t rs2;
  std::uint16_t i;
  std::uint16_t j;
  std::uint8_t loop_id;
};

constexpr std::uint64_t normConfig(std::uint8_t stat_id) {
  return 3u | (std::uint64_t{stat_id} << 8) | (std::uint64_t{1} << 17);
}

class StCDriver : public Component {
  DECLARE_COMPONENT(StCDriver);

 public:
  StCDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, req_val);
  Input(bit, req_rdy);
  Output(smesh::LoopMatmulStCReq, req_bits);
  Input(bit, cmd_val);
  Output(bit, cmd_rdy);
  Input(smesh::SmeshCmd, cmd_bits);
  Output(u16, ex_k);
  Output(u16, ex_j);
  Output(u16, ex_i);
  Output(bit, ex_completed);
  Output(bit, st_utilization_at_limit);
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

  bool passed() const { return passed_ && observed_stall_ == 0x3u && saw_resadd_; }
  unsigned commandsAccepted() const { return command_pos_; }
  unsigned requestsAccepted() const { return static_cast<unsigned>(*request_pos_Q_); }

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, request_pos_Q_);
  Register(u8, request_pos_D_);

  std::array<smesh::LoopMatmulStCReq, 6> requests_{{
      {1, 2, 2, 1, 1, 0x1000, 16, 0, 0, 0, false, false},
      {1, 2, 1, 1, 0, 0x2000, 8, 0, 0, 1, true, false},
      {1, 2, 1, 0, 0, 0x2500, 8, 0, 0, 0, false, true},
      {1, 1, 1, 0, 1, 0x3000, 8, 0, 2, 1, false, false},
      // Exceeds the small test accumulator to exercise split-command generation only.
      {1, 17, 1, 1, 3, 0x4000, 8, 0, 4, 0, false, false},
      {1, 1, 1, 0, 0, 0, 0, 0, 0, 1, false, false},
  }};
  std::array<ExpectedCommand, 32> expected_{{
      {smesh::SmeshFunct::Mvout, 0x1000,
       smesh::packLocal(smesh::makeAccAddr(0), {4, 7}), 0, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x1040,
       smesh::packLocal(smesh::makeAccAddr(8), {3, 7}), 1, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x2000,
       smesh::packLocal(smesh::makeAccAddr(0, false, true), {4, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x2010,
       smesh::packLocal(smesh::makeAccAddr(4, false, true), {4, 3}), 0, 1, 1},
      {smesh::SmeshFunct::Mvout, 0x2500,
       smesh::packLocal(smesh::makeAccAddr(0), {4, 8}), 0, 0, 0},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3000,
       smesh::packLocal(smesh::makeAccAddr(0, false, false, 2), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(1), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3008,
       smesh::packLocal(smesh::makeAccAddr(1, false, false, 2), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3000,
       smesh::packLocal(smesh::makeAccAddr(0, false, false, 4), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(1), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3008,
       smesh::packLocal(smesh::makeAccAddr(1, false, false, 4), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3000,
       smesh::packLocal(smesh::makeAccAddr(0, false, false, 0), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(1), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3008,
       smesh::packLocal(smesh::makeAccAddr(1, false, false, 0), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3010,
       smesh::packLocal(smesh::makeAccAddr(2, false, false, 2), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3010,
       smesh::packLocal(smesh::makeAccAddr(2, false, false, 4), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 1},
      {smesh::SmeshFunct::Mvout, 0x3010,
       smesh::packLocal(smesh::makeAccAddr(2, false, false, 0), {1, 4}), 0, 0, 1},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x4000,
       smesh::packLocal(smesh::makeAccAddr(0, false, false, 5), {1, 64}), 0, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x4040,
       smesh::packLocal(smesh::makeAccAddr(16, false, false, 5), {1, 3}), 0, 16, 0},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x4000,
       smesh::packLocal(smesh::makeAccAddr(0, false, false, 6), {1, 64}), 0, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x4040,
       smesh::packLocal(smesh::makeAccAddr(16, false, false, 7), {1, 3}), 0, 16, 0},
      {smesh::SmeshFunct::Config, normConfig(0), 0, 0, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x4000,
       smesh::packLocal(smesh::makeAccAddr(0, false, false, 0), {1, 64}), 0, 0, 0},
      {smesh::SmeshFunct::Mvout, 0x4040,
       smesh::packLocal(smesh::makeAccAddr(16, false, false, 0), {1, 3}), 0, 16, 0},
  }};

  unsigned command_pos_ = 0;
  unsigned observed_stall_ = 0;
  bool saw_resadd_ = false;
  bool passed_ = true;
};

StCDriver::StCDriver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  request_pos_Q_ <= request_pos_D_;
  UPDATE(updateRequest).reads(request_pos_Q_).writes(req_val, req_bits);
  UPDATE(updateRequestIndex)
      .reads(request_pos_Q_, req_val, req_rdy).writes(request_pos_D_);
  UPDATE(updateDrive).reads(cycle_Q_)
      .writes(cmd_rdy, ex_k, ex_j, ex_i, ex_completed, st_utilization_at_limit);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
  UPDATE(observe)
      .reads(cycle_Q_, cmd_val, cmd_rdy, cmd_bits, i, j, idle, loop_id)
      .reads(req_rdy, ex_completed);
}

void StCDriver::updateRequest() {
  const auto pos = static_cast<unsigned>(*request_pos_Q_);
  req_val = bit(pos < requests_.size());
  req_bits = pos < requests_.size() ? requests_[pos] : smesh::LoopMatmulStCReq{};
}

void StCDriver::updateRequestIndex() {
  if (req_val == 1 && req_rdy == 1) {
    request_pos_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*request_pos_Q_) + 1);
  }
}

void StCDriver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  ex_k = 0;
  ex_j = bit(cycle == 2);
  ex_i = bit(cycle == 2 || cycle == 11);
  ex_completed = bit(cycle >= 4 && cycle != 11);
  st_utilization_at_limit = bit(cycle == 3);
  cmd_rdy = bit(cycle >= 5);
}

void StCDriver::updateCycle() {
  cycle_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*cycle_Q_) + 1);
}

void StCDriver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle == 1 && idle == 0) {
    passed_ &= cmd_val == 0;
    observed_stall_ |= 1u;
  }
  if ((cycle == 2 || cycle == 4) && idle == 0) {
    passed_ &= cmd_val == 1 && cmd_rdy == 0;
    observed_stall_ |= 2u;
  }
  if (cycle == 3 && idle == 0) passed_ &= cmd_val == 0;
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
                         i == expected.i && j == expected.j &&
                         loop_id == expected.loop_id && idle == 0 && req_rdy == 0;
      if (!match) {
        std::printf("StC command %u mismatch: funct=%u rs1=0x%llx rs2=0x%llx ij={%u,%u}\n",
                    command_pos_, static_cast<unsigned>(actual.funct),
                    static_cast<unsigned long long>(actual.rs1),
                    static_cast<unsigned long long>(actual.rs2),
                    static_cast<unsigned>(*i), static_cast<unsigned>(*j));
      }
      passed_ &= match;
    }
    if (ex_completed == 0 && command_pos_ == 4) saw_resadd_ = true;
    ++command_pos_;
  }
}

void StCDriver::reset() {
  cycle_D_.reset(0);
  request_pos_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::LoopMatmulStCReq{});
  cmd_rdy.reset(0);
  ex_k.reset(0);
  ex_j.reset(0);
  ex_i.reset(0);
  ex_completed.reset(0);
  st_utilization_at_limit.reset(0);
  command_pos_ = 0;
  observed_stall_ = 0;
  saw_resadd_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  StCDriver driver("Driver");
  smesh::LoopMatmulStC st_c("StC");
  st_c.req_val << driver.req_val;
  st_c.req_bits << driver.req_bits;
  driver.req_rdy << st_c.req_rdy;
  st_c.cmd_rdy << driver.cmd_rdy;
  driver.cmd_val << st_c.cmd_val;
  driver.cmd_bits << st_c.cmd_bits;
  st_c.ex_k << driver.ex_k;
  st_c.ex_j << driver.ex_j;
  st_c.ex_i << driver.ex_i;
  st_c.ex_completed << driver.ex_completed;
  st_c.st_utilization_at_limit << driver.st_utilization_at_limit;
  driver.i << st_c.i;
  driver.j << st_c.j;
  driver.idle << st_c.idle;
  driver.loop_id << st_c.loop_id;

  Clock clk;
  driver.clk << clk;
  st_c.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  bool passed = *st_c.idle == 1 && *st_c.req_rdy == 1 && *st_c.cmd_val == 0;
  for (int cycle = 0; cycle < 55; ++cycle) Sim::run();
  passed &= driver.passed() && driver.requestsAccepted() == 6 &&
            driver.commandsAccepted() == 32 && *st_c.idle == 1;

  descore::flushLog();
  std::printf("[LOOP_MATMUL_ST_C] %s requests=%u commands=%u\n",
              passed ? "PASS" : "FAIL", driver.requestsAccepted(), driver.commandsAccepted());
  return passed ? 0 : 1;
}
