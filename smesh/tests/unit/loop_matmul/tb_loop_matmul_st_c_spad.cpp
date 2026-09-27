// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_st_c_spad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulStCSpad.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

struct Expected {
  std::uint64_t rs1;
  std::uint64_t rs2;
  std::uint16_t i;
  std::uint16_t j;
  std::uint8_t loop_id;
};

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, req_val);
  Input(bit, req_rdy);
  Output(smesh::LoopMatmulStCSpadReq, req_bits);
  Input(bit, cmd_val);
  Output(bit, cmd_rdy);
  Input(smesh::SmeshCmd, cmd_bits);
  Output(u16, ex_k);
  Output(u16, ex_j);
  Output(u16, ex_i);
  Output(bit, ex_completed);
  Output(bit, st_utilization_at_limit);
  Input(u16, i);
  Input(u16, j);
  Input(bit, idle);
  Input(u8, loop_id);

  void driveRequest();
  void advanceRequest();
  void driveControl();
  void advanceCycle();
  void observe();
  void reset();
  bool passed() const { return passed_ && saw_wait_ && saw_limit_; }
  unsigned sent() const { return sent_; }
  unsigned accepted() const { return static_cast<unsigned>(*request_Q_); }

 private:
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  Output(u8, request_Q_);
  Register(u8, request_D_);

  const std::array<smesh::LoopMatmulStCSpadReq, 3> requests_{{
      {1, 2, 2, 1, 1, 0, 0, false, 0, 0, false},
      {1, 1, 1, 0, 0, 8, 8, true, 2, 1, false},
      {2, 1, 1, 0, 0, 12, 12, false, 0, 0, true},
  }};
  const std::array<Expected, 6> expected_{{
      {smesh::packStoreSpadDestination(smesh::makeSpAddr(0)),
       smesh::packLocal(smesh::makeAccAddr(0), {4, 4}), 0, 0, 0},
      {smesh::packStoreSpadDestination(smesh::makeSpAddr(8)),
       smesh::packLocal(smesh::makeAccAddr(8), {3, 4}), 1, 0, 0},
      {smesh::packStoreSpadDestination(smesh::makeSpAddr(4)),
       smesh::packLocal(smesh::makeAccAddr(4), {4, 3}), 0, 1, 0},
      {smesh::packStoreSpadDestination(smesh::makeSpAddr(12)),
       smesh::packLocal(smesh::makeAccAddr(12), {3, 3}), 1, 1, 0},
      {smesh::packStoreSpadDestination(smesh::makeSpAddr(8)),
       smesh::packLocal(smesh::makeAccAddr(8, false, true), {4, 4}), 0, 0, 1},
      {smesh::packStoreSpadDestination(smesh::makeSpAddr(12)),
       smesh::packLocal(smesh::makeAccAddr(12), {4, 4}), 0, 0, 0},
  }};
  unsigned sent_ = 0;
  bool passed_ = true;
  bool saw_wait_ = false;
  bool saw_limit_ = false;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  request_Q_ <= request_D_;
  UPDATE(driveRequest).reads(request_Q_).writes(req_val, req_bits);
  UPDATE(advanceRequest).reads(request_Q_, req_val, req_rdy).writes(request_D_);
  UPDATE(driveControl).reads(cycle_Q_, request_Q_)
      .writes(cmd_rdy, ex_k, ex_j, ex_i, ex_completed, st_utilization_at_limit);
  UPDATE(advanceCycle).reads(cycle_Q_).writes(cycle_D_);
  UPDATE(observe)
      .reads(cycle_Q_, cmd_val, cmd_rdy, cmd_bits, i, j, idle, loop_id)
      .reads(st_utilization_at_limit);
}

void Driver::driveRequest() {
  const auto pos = static_cast<unsigned>(*request_Q_);
  req_val = bit(pos < requests_.size());
  req_bits = pos < requests_.size() ? requests_[pos] : smesh::LoopMatmulStCSpadReq{};
}

void Driver::advanceRequest() {
  if (req_val == 1 && req_rdy == 1)
    request_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*request_Q_) + 1);
}

void Driver::driveControl() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto request = static_cast<unsigned>(*request_Q_);
  cmd_rdy = bit(cycle >= 7);
  st_utilization_at_limit = bit(cycle == 5);
  ex_completed = bit(request < 3 && cycle >= 4);
  ex_k = 0;
  ex_j = bit(request == 3);
  ex_i = 0;
}

void Driver::advanceCycle() {
  cycle_D_ = static_cast<std::uint8_t>(static_cast<unsigned>(*cycle_Q_) + 1);
}

void Driver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle == 2 && idle == 0) {
    passed_ &= cmd_val == 0;
    saw_wait_ = true;
  }
  if (st_utilization_at_limit == 1 && idle == 0) {
    passed_ &= cmd_val == 0;
    saw_limit_ = true;
  }
  if (cmd_val == 1 && cmd_rdy == 1) {
    if (sent_ >= expected_.size()) {
      passed_ = false;
    } else {
      const auto actual = *cmd_bits;
      const auto& want = expected_[sent_];
      const bool match = static_cast<unsigned>(actual.funct) ==
                             static_cast<unsigned>(smesh::SmeshFunct::StoreSpad) &&
                         static_cast<std::uint64_t>(actual.rs1) == want.rs1 &&
                         static_cast<std::uint64_t>(actual.rs2) == want.rs2 &&
                         i == want.i && j == want.j && loop_id == want.loop_id;
      if (!match)
        std::printf("StCSpad command %u mismatch: rs1=0x%llx rs2=0x%llx ij={%u,%u}\n",
                    sent_, static_cast<unsigned long long>(actual.rs1),
                    static_cast<unsigned long long>(actual.rs2),
                    static_cast<unsigned>(*i), static_cast<unsigned>(*j));
      passed_ &= match;
    }
    ++sent_;
  }
}

void Driver::reset() {
  cycle_D_.reset(0);
  request_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::LoopMatmulStCSpadReq{});
  cmd_rdy.reset(0);
  ex_k.reset(0);
  ex_j.reset(0);
  ex_i.reset(0);
  ex_completed.reset(0);
  st_utilization_at_limit.reset(0);
  sent_ = 0;
  passed_ = true;
  saw_wait_ = false;
  saw_limit_ = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::LoopMatmulStCSpad st("StCSpad");
  st.req_val << driver.req_val;
  st.req_bits << driver.req_bits;
  driver.req_rdy << st.req_rdy;
  st.cmd_rdy << driver.cmd_rdy;
  driver.cmd_val << st.cmd_val;
  driver.cmd_bits << st.cmd_bits;
  st.ex_k << driver.ex_k;
  st.ex_j << driver.ex_j;
  st.ex_i << driver.ex_i;
  st.ex_completed << driver.ex_completed;
  st.st_utilization_at_limit << driver.st_utilization_at_limit;
  driver.i << st.i;
  driver.j << st.j;
  driver.idle << st.idle;
  driver.loop_id << st.loop_id;

  Clock clk;
  driver.clk << clk;
  st.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  bool passed = *st.idle == 1 && *st.req_rdy == 1 && *st.cmd_val == 0;
  for (int cycle = 0; cycle < 24; ++cycle) Sim::run();
  passed &= driver.passed() && driver.accepted() == 3 &&
            driver.sent() == 6 && *st.idle == 1;
  descore::flushLog();
  std::printf("[LOOP_MATMUL_ST_C_SPAD] %s requests=%u commands=%u\n",
              passed ? "PASS" : "FAIL", driver.accepted(), driver.sent());
  return passed ? 0 : 1;
}
