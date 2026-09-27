// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_ld_a.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
// 

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmulLdA.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

struct ExpectedCommand {
  std::uint64_t dram_addr;
  std::uint64_t local;
  std::uint16_t i;
  std::uint16_t k;
  std::uint8_t loop_id;
};

class LdADriver : public Component {
  DECLARE_COMPONENT(LdADriver);

 public:
  LdADriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, req_val);
  Input(bit, req_rdy);
  Output(smesh::LoopMatmulLdAReq, req_bits);
  Input(bit, cmd_val);
  Output(bit, cmd_rdy);
  Input(smesh::SmeshCmd, cmd_bits);
  Output(bit, ld_utilization_at_limit);
  Input(u16, i);
  Input(u16, k);
  Input(bit, idle);
  Input(u8, loop_id);

  void updateRequest();
  void updateBackpressure();
  void observe();
  void reset();

  bool passed() const { return passed_; }
  std::size_t requestsAccepted() const { return request_pos_; }
  std::size_t commandsAccepted() const { return command_pos_; }
  bool sawLimit() const { return saw_limit_; }
  bool sawCommandStall() const { return saw_command_stall_; }

 private:
  std::array<smesh::LoopMatmulLdAReq, 5> requests_{{
      {2, 2, 1, 1, 0x1000, 16, 0, 0, false, false},
      {2, 2, 0, 1, 0x2000, 32, 0, 1, true, false},
      {1, 1, 0, 0, 0, 0, 0, 0, false, false},
      {1, 1, 0, 0, 0x3000, 0, 4, 0, false, true},
      {1, 17, 0, 0, 0x4000, 20, 32, 1, false, false},
  }};
  std::array<ExpectedCommand, 7> expected_{{
      {0x1000, smesh::packLocal(smesh::makeSpAddr(0), {4, 7}), 0, 0, 0},
      {0x1040, smesh::packLocal(smesh::makeSpAddr(8), {3, 7}), 1, 0, 0},
      {0x2000, smesh::packLocal(smesh::makeSpAddr(0), {4, 8}), 0, 0, 1},
      {0x2080, smesh::packLocal(smesh::makeSpAddr(8), {3, 8}), 0, 1, 1},
      {0x3000, smesh::packLocal(smesh::makeAccAddr(4), {4, 4}), 0, 0, 0},
      {0x4000, smesh::packLocal(smesh::makeSpAddr(32), {4, 64}), 0, 0, 1},
      {0x4040, smesh::packLocal(smesh::makeSpAddr(96), {4, 4}), 0, 16, 1},
  }};

  std::size_t request_pos_ = 0;
  std::size_t command_pos_ = 0;
  unsigned cycle_ = 0;
  bool passed_ = true;
  bool saw_limit_ = false;
  bool saw_command_stall_ = false;
};

LdADriver::LdADriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateRequest).reads(req_rdy).writes(req_val, req_bits);
  UPDATE(updateBackpressure).writes(cmd_rdy, ld_utilization_at_limit);
  UPDATE(observe).reads(cmd_val, cmd_bits, cmd_rdy, ld_utilization_at_limit,
                        req_rdy, i, k, idle).reads(loop_id);
}

void LdADriver::updateRequest() {
  if (Sim::state == Sim::SimResetting) {
    req_val = 0;
    req_bits = smesh::LoopMatmulLdAReq{};
    return;
  }
  const bool offering = request_pos_ < requests_.size();
  req_val = bit(offering);
  req_bits = offering ? requests_[request_pos_] : smesh::LoopMatmulLdAReq{};
  if (offering && req_rdy == 1) ++request_pos_;
}

void LdADriver::updateBackpressure() {
  ld_utilization_at_limit = bit(Sim::state != Sim::SimResetting && cycle_ < 4);
  cmd_rdy = bit(Sim::state != Sim::SimResetting && (cycle_ < 4 || cycle_ >= 6));
}

void LdADriver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  if (idle == 0 && ld_utilization_at_limit == 1) {
    saw_limit_ = true;
    passed_ &= cmd_val == 0 && i == 0 && k == 0;
  }
  if (cmd_val == 1 && cmd_rdy == 0) {
    saw_command_stall_ = true;
    passed_ &= i == 0 && k == 0;
  }
  if (cmd_val == 1 && cmd_rdy == 1) {
    if (command_pos_ >= expected_.size()) {
      passed_ = false;
    } else {
      const auto actual = *cmd_bits;
      const auto& expected = expected_[command_pos_];
      const bool match = static_cast<std::uint32_t>(actual.funct) ==
                             static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin) &&
                         static_cast<std::uint64_t>(actual.rs1) == expected.dram_addr &&
                         static_cast<std::uint64_t>(actual.rs2) == expected.local &&
                         i == expected.i && k == expected.k && loop_id == expected.loop_id &&
                         idle == 0 && req_rdy == 0;
      if (!match) {
        std::printf("LdA command %zu mismatch: rs1=0x%llx rs2=0x%llx i=%u k=%u loop=%u\n",
                    command_pos_, static_cast<unsigned long long>(actual.rs1),
                    static_cast<unsigned long long>(actual.rs2),
                    static_cast<unsigned>(*i), static_cast<unsigned>(*k),
                    static_cast<unsigned>(*loop_id));
      }
      passed_ &= match;
    }
    ++command_pos_;
  }
  ++cycle_;
}

void LdADriver::reset() {
  request_pos_ = 0;
  command_pos_ = 0;
  cycle_ = 0;
  passed_ = true;
  saw_limit_ = false;
  saw_command_stall_ = false;
  req_val.reset(0);
  req_bits.reset(smesh::LoopMatmulLdAReq{});
  cmd_rdy.reset(0);
  ld_utilization_at_limit.reset(0);
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  LdADriver driver("Driver");
  smesh::LoopMatmulLdA ld_a("LdA");
  ld_a.req_val << driver.req_val;
  ld_a.req_bits << driver.req_bits;
  driver.req_rdy << ld_a.req_rdy;
  ld_a.cmd_rdy << driver.cmd_rdy;
  ld_a.ld_utilization_at_limit << driver.ld_utilization_at_limit;
  driver.cmd_val << ld_a.cmd_val;
  driver.cmd_bits << ld_a.cmd_bits;
  driver.i << ld_a.i;
  driver.k << ld_a.k;
  driver.idle << ld_a.idle;
  driver.loop_id << ld_a.loop_id;

  Clock clk;
  driver.clk << clk;
  ld_a.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  bool passed = *ld_a.idle == 1 && *ld_a.req_rdy == 1 && *ld_a.cmd_val == 0;
  for (int cycle = 0; cycle < 40; ++cycle) Sim::run();
  passed &= driver.passed() && driver.requestsAccepted() == 5 &&
            driver.commandsAccepted() == 7 && driver.sawLimit() &&
            driver.sawCommandStall() && *ld_a.idle == 1;

  descore::flushLog();
  std::printf("[LOOP_MATMUL_LD_A] %s requests=%zu commands=%zu\n",
              passed ? "PASS" : "FAIL", driver.requestsAccepted(), driver.commandsAccepted());
  return passed ? 0 : 1;
}
