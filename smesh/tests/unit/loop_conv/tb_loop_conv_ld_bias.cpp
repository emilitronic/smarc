// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_ld_bias.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConvLdBias.hpp"

#include <array>
#include <cstdio>

namespace {

smesh::SmeshCmd command(unsigned funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshCmd value{};
  value.funct = funct;
  value.rs1 = rs1;
  value.rs2 = rs2;
  return value;
}

bool same(const smesh::SmeshCmd& a, const smesh::SmeshCmd& b) {
  return a.funct == b.funct && a.rs1 == b.rs1 && a.rs2 == b.rs2;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::LoopConvLdBiasReq, req_bits);
  Input(bit, req_rdy);
  Output(bit, cmd_rdy);
  Input(bit, cmd_val);
  Input(smesh::SmeshCmd, cmd_bits);
  Output(bit, ld_utilization_at_limit);
  Output(bit, wait_for_prev_loop);
  Input(bit, idle);
  Input(u8, loop_id);

  void updateDrive();
  void updateRequests();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const {
    return passed_ && *request_Q_ == 4 && seen_ == expected_.size() &&
           saw_stall_ && saw_pressure_ && first_visible_ == 6 && *idle == 1;
  }

 private:
  std::array<smesh::LoopConvLdBiasReq, 4> requests_{};
  std::array<smesh::SmeshCmd, 13> expected_{};
  Output(u8, request_Q_);
  Register(u8, request_D_);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  bool passed_ = true;
  unsigned seen_ = 0;
  unsigned first_visible_ = 999;
  bool stalled_ = false;
  smesh::SmeshCmd stalled_command_{};
  bool saw_stall_ = false;
  bool saw_pressure_ = false;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  // Channel tail: 17 channels are loaded as 16 then 1, from the same bias vector.
  requests_[0].inner_bounds.batches = 1;
  requests_[0].inner_bounds.orows = 1;
  requests_[0].inner_bounds.ocols = 1;
  requests_[0].ochs = 17;
  requests_[0].bias_spad_stride = 1;
  requests_[0].dram_addr = 0x80004000;

  // Column tail and two output rows.
  requests_[1].inner_bounds.batches = 1;
  requests_[1].inner_bounds.orows = 2;
  requests_[1].inner_bounds.ocols = 5;
  requests_[1].ochs = 3;
  requests_[1].bias_spad_stride = 10;
  requests_[1].dram_addr = 0x80005000;
  requests_[1].loop_id = 1;

  // No bias: a nonzero source address keeps the job active, but LOAD3 uses zero.
  requests_[2].inner_bounds.batches = 2;
  requests_[2].inner_bounds.orows = 2;
  requests_[2].inner_bounds.ocols = 1;
  requests_[2].ochs = 3;
  requests_[2].bias_spad_stride = 4;
  requests_[2].addr_start = 8;
  requests_[2].dram_addr = 0x80006000;
  requests_[2].no_bias = 1;
  // Zero address: skip the entire fourth job, including CONFIG_LOAD.
  requests_[3].loop_id = 1;

  // Literal encodings independently check scale, load-state ID, address flags and shapes.
  expected_ = {{
    command(0, 0x3f80000000010111ull, 0),
    command(14, 0x80004000, 0x0001001080000000ull),
    command(14, 0x80004040, 0x0001000180000004ull),
    command(0, 0x3f800000000a0111ull, 0),
    command(14, 0x80005000, 0x0004000380000000ull),
    command(14, 0x80005000, 0x0001000380000004ull),
    command(14, 0x80005000, 0x0004000380000005ull),
    command(14, 0x80005000, 0x0001000380000009ull),
    command(0, 0x3f80000000040111ull, 0),
    command(14, 0, 0x0001000380000008ull),
    command(14, 0, 0x0001000380000009ull),
    command(14, 0, 0x000100038000000aull),
    command(14, 0, 0x000100038000000bull),
  }};

  request_Q_ <= request_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(request_Q_, cycle_Q_)
      .writes(req_val, req_bits, cmd_rdy, ld_utilization_at_limit, wait_for_prev_loop);
  UPDATE(updateRequests).reads(request_Q_, req_val, req_rdy).writes(request_D_);
  UPDATE(updateCheck).reads(cycle_Q_, request_Q_, req_val, req_rdy, cmd_val, cmd_bits)
      .reads(cmd_rdy, ld_utilization_at_limit, idle, loop_id);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

void Driver::updateDrive() {
  const auto index = static_cast<unsigned>(*request_Q_);
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  req_val = bit(index < requests_.size());
  req_bits = index < requests_.size() ? requests_[index] : smesh::LoopConvLdBiasReq{};
  wait_for_prev_loop = bit((cycle >= 1 && cycle <= 3) || (cycle >= 20 && cycle <= 23));
  cmd_rdy = bit(!(cycle >= 6 && cycle <= 8) && cycle % 5 != 0);
  ld_utilization_at_limit = bit(cycle >= 9 && cycle <= 11);
}

void Driver::updateRequests() {
  if (req_val == 1 && req_rdy == 1) request_D_ = static_cast<unsigned>(*request_Q_) + 1;
}

// Observe all output transfers and ensure a stalled command remains unchanged.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto actual = *cmd_bits;
  passed_ &= req_rdy == idle;
  if (ld_utilization_at_limit == 1) {
    passed_ &= cmd_val == 0;
    saw_pressure_ = true;
  }
  if (cmd_val == 1) {
    if (first_visible_ == 999) first_visible_ = cycle;
    passed_ &= idle == 0 && req_rdy == 0;
    if (stalled_) passed_ &= same(actual, stalled_command_);
    if (cmd_rdy == 0) {
      stalled_ = true;
      stalled_command_ = actual;
      saw_stall_ = true;
    } else {
      stalled_ = false;
      const bool match = seen_ < expected_.size() && same(actual, expected_[seen_]);
      if (!match)
        std::printf("Bias command %u mismatch funct=%u rs1=0x%llx rs2=0x%llx\n",
                    seen_, static_cast<unsigned>(actual.funct),
                    static_cast<unsigned long long>(actual.rs1),
                    static_cast<unsigned long long>(actual.rs2));
      passed_ &= match;
      const unsigned expected_loop = seen_ >= 3 && seen_ < 8 ? 1 : 0;
      passed_ &= loop_id == expected_loop;
      ++seen_;
    }
  }
  if (req_val == 1 && req_rdy == 1) {
    constexpr std::array<unsigned, 4> previous_commands{{0, 3, 8, 13}};
    const auto index = static_cast<unsigned>(*request_Q_);
    passed_ &= index < previous_commands.size() && seen_ == previous_commands[index];
  }
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void Driver::reset() {
  request_D_.reset(0);
  cycle_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::LoopConvLdBiasReq{});
  cmd_rdy.reset(0);
  ld_utilization_at_limit.reset(0);
  wait_for_prev_loop.reset(0);
  passed_ = true;
  seen_ = 0;
  first_visible_ = 999;
  stalled_ = false;
  saw_stall_ = false;
  saw_pressure_ = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConvLdBias worker("LdBias");
  worker.req_val << driver.req_val;
  worker.req_bits << driver.req_bits;
  driver.req_rdy << worker.req_rdy;
  driver.cmd_val << worker.cmd_val;
  driver.cmd_bits << worker.cmd_bits;
  worker.cmd_rdy << driver.cmd_rdy;
  worker.ld_utilization_at_limit << driver.ld_utilization_at_limit;
  worker.wait_for_prev_loop << driver.wait_for_prev_loop;
  driver.idle << worker.idle;
  driver.loop_id << worker.loop_id;
  Clock clk;
  driver.clk << clk;
  worker.clk << clk;
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
  std::printf("[LOOP_CONV_LD_BIAS] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
