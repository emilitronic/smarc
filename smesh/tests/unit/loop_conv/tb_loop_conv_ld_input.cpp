// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_ld_input.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConvLdInput.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace {

struct Expected {
  smesh::SmeshCmd command{};
  unsigned loop_id = 0;
};

bool same(const smesh::SmeshCmd& a, const smesh::SmeshCmd& b) {
  return a.funct == b.funct && a.rs1 == b.rs1 && a.rs2 == b.rs2;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::LoopConvLdInputReq, req_bits);
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
    return passed_ && *request_Q_ == requests_.size() && seen_ == expected_.size() &&
           first_visible_ == 6 && saw_stall_ && saw_pressure_ && *idle == 1;
  }
 private:
  std::array<smesh::LoopConvLdInputReq, 8> requests_{};
  std::vector<Expected> expected_;
  std::array<unsigned, 8> boundaries_{};
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
  for (unsigned i = 0; i < requests_.size(); ++i) {
    auto& req = requests_[i];
    req.dram_addr = 0x80001000 + i * 0x1000;
    req.loop_id = i % 2;
    req.max_pixels_per_row = 1;
    req.inner_bounds.batches = 1;
    req.outer_bounds.in_row_dim = 1;
    req.outer_bounds.in_stride = 1;
  }
  auto& normal = requests_[0];
  normal.outer_bounds.in_col_dim = 5;
  normal.outer_bounds.in_stride = 3;
  normal.geometry = {1, 5, 1, 5, 3, 5};

  auto& padded = requests_[1];
  padded.outer_bounds.in_col_dim = 2;
  padded.inner_bounds.upad = padded.inner_bounds.dpad = 1;
  padded.inner_bounds.lpad = padded.inner_bounds.rpad = 1;
  padded.geometry = {3, 4, 1, 2, 1, 12};

  auto& transposed = requests_[2];
  transposed.outer_bounds.batch_size = 3;
  transposed.outer_bounds.in_col_dim = 2;
  transposed.inner_bounds.batches = 3;
  transposed.trans_input_3120 = 1;
  transposed.geometry = {1, 2, 1, 2, 2, 4};

  auto& downsampled = requests_[3];
  downsampled.outer_bounds.in_row_dim = 4;
  downsampled.outer_bounds.in_col_dim = 4;
  downsampled.downsample = 1;
  downsampled.geometry = {4, 4, 4, 4, 1, 4};

  auto& dilated = requests_[4];
  dilated.outer_bounds.in_col_dim = 1;
  dilated.inner_bounds.upad = dilated.inner_bounds.lpad = dilated.inner_bounds.rpad = 1;
  dilated.input_dilated = 1;
  dilated.max_pixels_per_row = 2;
  dilated.geometry = {2, 3, 1, 1, 1, 6};

  // Arithmetic-only boundary test: the second channel block wraps the SPAD address.
  auto& channels = requests_[5];
  channels.outer_bounds.in_col_dim = 1;
  channels.outer_bounds.in_stride = 65;
  channels.geometry = {1, 1, 1, 1, 65, 1};
  auto& batches = requests_[6];
  batches.outer_bounds.in_col_dim = 1;
  batches.inner_bounds.batches = 2;
  batches.addr_start = 8;
  batches.geometry = {1, 1, 1, 1, 1, 2};
  requests_[7].dram_addr = 0; // Skip the entire final job.

  const auto add = [&](unsigned job, unsigned funct, std::uint64_t rs1, std::uint64_t rs2) {
    Expected expected{};
    expected.command.funct = funct;
    expected.command.rs1 = rs1;
    expected.command.rs2 = rs2;
    expected.loop_id = job % 2;
    expected_.push_back(expected);
  };
  // Independent literal commands: CONFIG_LOAD slot 0, signed-padding zero loads,
  // shapes, local addresses, transpose offsets, and sampling strides.
  boundaries_[0] = expected_.size();
  add(0, 0, 0x3f80000000050101ull, 3);
  add(0, 2, 0x80001000, 0x0004000300000000ull);
  add(0, 2, 0x8000100c, 0x0001000300000004ull);
  boundaries_[1] = expected_.size();
  add(1, 0, 0x3f800000000c0101ull, 1);
  add(1, 2, 0, 0x0001000100000000ull);
  add(1, 2, 0, 0x0002000100000001ull);
  add(1, 2, 0, 0x0001000100000003ull);
  add(1, 2, 0, 0x0001000100000004ull);
  add(1, 2, 0x80002000, 0x0002000100000005ull);
  add(1, 2, 0, 0x0001000100000007ull);
  add(1, 2, 0, 0x0001000100000008ull);
  add(1, 2, 0, 0x0002000100000009ull);
  add(1, 2, 0, 0x000100010000000bull);
  boundaries_[2] = expected_.size();
  add(2, 0, 0x3f80000000040101ull, 3);
  add(2, 2, 0x80003000, 0x0002000300000000ull);
  add(2, 2, 0x80003006, 0x0002000300000002ull);
  boundaries_[3] = expected_.size();
  add(3, 0, 0x3f80000000040101ull, 2);
  add(3, 2, 0x80004000, 0x0002000100000000ull);
  add(3, 2, 0x80004008, 0x0002000100000002ull);
  boundaries_[4] = expected_.size();
  add(4, 0, 0x3f80000000060201ull, 1);
  add(4, 2, 0, 0x0001000100000000ull);
  add(4, 2, 0, 0x0001000100000001ull);
  add(4, 2, 0, 0x0001000100000002ull);
  add(4, 2, 0, 0x0001000100000003ull);
  add(4, 2, 0x80005000, 0x0001000100000004ull);
  add(4, 2, 0, 0x0001000100000005ull);
  boundaries_[5] = expected_.size();
  add(5, 0, 0x3f80000000010101ull, 65);
  add(5, 2, 0x80006000, 0x0001004000000000ull);
  add(5, 2, 0x80006040, 0x0001000100000000ull);
  boundaries_[6] = expected_.size();
  add(6, 0, 0x3f80000000020101ull, 1);
  add(6, 2, 0x80007000, 0x0001000100000008ull);
  add(6, 2, 0x80007001, 0x0001000100000009ull);
  boundaries_[7] = expected_.size();

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
  const auto request = static_cast<unsigned>(*request_Q_);
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  req_val = bit(request < requests_.size());
  req_bits = request < requests_.size() ? requests_[request] : smesh::LoopConvLdInputReq{};
  wait_for_prev_loop = bit((cycle >= 1 && cycle <= 3) || (cycle >= 20 && cycle <= 23));
  cmd_rdy = bit(!(cycle >= 6 && cycle <= 8) && cycle % 5 != 0);
  ld_utilization_at_limit = bit(cycle >= 9 && cycle <= 11);
}

void Driver::updateRequests() {
  if (req_val == 1 && req_rdy == 1) request_D_ = static_cast<unsigned>(*request_Q_) + 1;
}

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
      const bool match = seen_ < expected_.size() &&
          same(actual, expected_[seen_].command) && loop_id == expected_[seen_].loop_id;
      if (!match)
        std::printf("Input command %u mismatch funct=%u rs1=0x%llx rs2=0x%llx\n",
                    seen_, static_cast<unsigned>(actual.funct),
                    static_cast<unsigned long long>(actual.rs1),
                    static_cast<unsigned long long>(actual.rs2));
      passed_ &= match;
      ++seen_;
    }
  }
  if (req_val == 1 && req_rdy == 1) {
    const auto request = static_cast<unsigned>(*request_Q_);
    passed_ &= request < boundaries_.size() && seen_ == boundaries_[request];
  }
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void Driver::reset() {
  request_D_.reset(0);
  cycle_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::LoopConvLdInputReq{});
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
  smesh::LoopConvLdInput worker("LdInput");
  worker.req_val << driver.req_val;
  worker.req_bits << driver.req_bits;
  driver.req_rdy << worker.req_rdy;
  worker.cmd_rdy << driver.cmd_rdy;
  driver.cmd_val << worker.cmd_val;
  driver.cmd_bits << worker.cmd_bits;
  worker.wait_for_prev_loop << driver.wait_for_prev_loop;
  worker.ld_utilization_at_limit << driver.ld_utilization_at_limit;
  driver.idle << worker.idle;
  driver.loop_id << worker.loop_id;
  Clock clk;
  driver.clk << clk;
  worker.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 200; ++cycle) Sim::run();
  bool passed = driver.passed();
  Sim::reset();
  for (int cycle = 0; cycle < 200; ++cycle) Sim::run();
  passed &= driver.passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_LD_INPUT] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
