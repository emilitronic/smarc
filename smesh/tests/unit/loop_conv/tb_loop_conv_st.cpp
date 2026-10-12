// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_st.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "LoopConvSt.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

struct Test {
  smesh::LoopConvStReq req{};
  std::vector<smesh::SmeshCmd> expected;
};

smesh::SmeshCmd command(unsigned funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshCmd value{};
  value.funct = funct; value.rs1 = rs1; value.rs2 = rs2;
  return value;
}

bool same(const smesh::SmeshCmd& a, const smesh::SmeshCmd& b) {
  return a.funct == b.funct && a.rs1 == b.rs1 && a.rs2 == b.rs2;
}

// Independently enumerate ordinary stores in batch/row/column/channel order.
// Oversized cases test encoded address arithmetic, not memory allocation legality.
Test ordinary(smesh::LoopConvStReq req) {
  Test test{req, {}};
  const auto& inner = req.inner_bounds;
  const auto& outer = req.outer_bounds;
  for (unsigned b = 0; b < inner.batches; ++b)
    for (unsigned row = 0; row < inner.orows; ++row)
      for (unsigned col = 0; col < inner.ocols; col += smesh::kDim)
        for (unsigned och = 0; och < req.ochs; och += smesh::kDim) {
          const auto element = req.trans_output_1203 == 1
              ? ((std::uint64_t{row} * outer.out_col_dim + col) * outer.batch_size + b) * outer.out_channels + och
              : (std::uint64_t{b} * outer.out_row_dim * outer.out_col_dim + row * outer.out_col_dim + col) * outer.out_stride + och;
          const auto local = req.addr_start + (och / smesh::kDim * inner.batches + b) *
                             inner.orows * inner.ocols + row * inner.ocols + col;
          test.expected.push_back(command(3, req.dram_addr + ((element * sizeof(smesh::Elem)) & 0xffffffffull),
              smesh::packLocal(smesh::makeAccAddr(local),
                {std::min<unsigned>(smesh::kDim, inner.ocols - col), std::min<unsigned>(smesh::kDim, req.ochs - och)})));
        }
  return test;
}

std::vector<Test> cases() {
  smesh::LoopConvStReq req{};
  req.no_pool = 1;
  req.dram_addr = 0x80004000;
  req.ochs = 5;
  req.inner_bounds.batches = req.inner_bounds.orows = 1;
  req.inner_bounds.ocols = 5;
  req.outer_bounds.batch_size = req.outer_bounds.out_row_dim = 1;
  req.outer_bounds.out_col_dim = req.outer_bounds.out_channels = 5;
  req.outer_bounds.out_stride = 7;
  std::vector<Test> tests{ordinary(req)};
  if (smesh::kDim == 4 && sizeof(smesh::Elem) == 1) {
    tests[0].expected = {
      command(3, 0x80004000, 0x0004000480000000ull),
      command(3, 0x80004004, 0x0004000180000005ull),
      command(3, 0x8000401c, 0x0001000480000004ull),
      command(3, 0x80004020, 0x0001000180000009ull),
    };
  }
  req.inner_bounds.batches = req.inner_bounds.orows = 2;
  req.addr_start = smesh::kAccRows / 2;
  req.outer_bounds.batch_size = 3;
  req.outer_bounds.out_channels = 7;
  req.outer_bounds.out_row_dim = 4;
  req.outer_bounds.out_col_dim = 9;
  tests.push_back(ordinary(req));
  req.trans_output_1203 = 1;
  tests.push_back(ordinary(req));
  // Large outer dimensions produce offsets above 32 bits: ordinary stores
  // must apply Original's offset mask before adding the 64-bit DRAM base.
  req.outer_bounds.out_col_dim = req.outer_bounds.batch_size = req.outer_bounds.out_channels = 65535;
  tests.push_back(ordinary(req));

  req = tests[0].req;
  req.no_pool = 0;
  req.activation = 1;
  req.inner_bounds.batches = 2;
  req.inner_bounds.orows = 3;
  req.inner_bounds.ocols = 5;
  req.inner_bounds.porows = 2;
  req.inner_bounds.pocols = 3;
  req.inner_bounds.plpad = 1;
  req.inner_bounds.pupad = 2;
  req.outer_bounds.pool_out_row_dim = 4;
  req.outer_bounds.pool_out_col_dim = 5;
  req.outer_bounds.pool_size = req.outer_bounds.pool_stride = 2;
  Test pool{req, {command(0, 0x05030302050006a6ull, 0xffffffff00000000ull | (7 * sizeof(smesh::Elem)))}};
  for (unsigned b = 0; b < 2; ++b)
    for (unsigned och = 0; och < 5; och += smesh::kDim) {
      const auto local = (och / smesh::kDim * 2 + b) * 15;
      pool.expected.push_back(command(3, req.dram_addr + (b * 140 + och) * sizeof(smesh::Elem),
          smesh::packLocal(smesh::makeAccAddr(local), {0, std::min<unsigned>(smesh::kDim, 5 - och)})));
    }
  pool.expected.push_back(command(0, 6, 0xffffffff00000000ull | (7 * sizeof(smesh::Elem))));
  tests.push_back(pool);
  // Pooling ignores trans_output_1203 and keeps its own batch-stride formula.
  pool.req.trans_output_1203 = 1;
  tests.push_back(pool);
  // Pool offsets are not limited to 32 bits, unlike ordinary-store offsets.
  pool.req.outer_bounds.pool_out_row_dim = pool.req.outer_bounds.pool_out_col_dim = 65535;
  pool.expected[0].rs1 = 0x05030302ff0006a6ull; // pool_out_dim truncates to 8 bits
  for (unsigned n = 1; n + 1 < pool.expected.size(); ++n) {
    const auto job = n - 1;
    const auto b = job / ((5 + smesh::kDim - 1) / smesh::kDim);
    const auto och = job % ((5 + smesh::kDim - 1) / smesh::kDim) * smesh::kDim;
    pool.expected[n].rs1 = req.dram_addr + (std::uint64_t{b} * 65535 * 65535 * 7 + och) * sizeof(smesh::Elem);
  }
  tests.push_back(pool);
  Test skipped{};
  skipped.req.no_pool = 1;
  tests.push_back(skipped); // Zero destination skips ordinary stores.
  skipped.req.no_pool = 0;
  tests.push_back(skipped); // Also skips both pooling configurations.
  for (unsigned i = 0; i < tests.size(); ++i) tests[i].req.loop_id = i % 2;
  return tests;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::LoopConvStReq, req_bits);
  Input(bit, req_rdy);
  Output(bit, cmd_rdy);
  Input(bit, cmd_val);
  Input(smesh::SmeshCmd, cmd_bits);
  Output(bit, ex_completed);
  Output(bit, st_utilization_at_limit);
  Input(bit, idle);
  Input(u8, loop_id);
  void updateDrive();
  void updateRequests();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const {
    return passed_ && request_Q_ == tests_.size() && seen_ == total_ && idle == 1 &&
           saw_stall_ && saw_pressure_ && first_visible_ == 6;
  }
 private:
  const std::vector<Test> tests_ = cases();
  unsigned total_ = 0;
  Output(u8, request_Q_);
  Register(u8, request_D_);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  bool passed_ = true;
  unsigned seen_ = 0;
  unsigned first_visible_ = 999;
  bool stalled_ = false;
  bool saw_stall_ = false;
  bool saw_pressure_ = false;
  smesh::SmeshCmd stalled_command_{};
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  for (const auto& test : tests_) total_ += test.expected.size();
  request_Q_ <= request_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(request_Q_, cycle_Q_).writes(req_val, req_bits, cmd_rdy, ex_completed, st_utilization_at_limit);
  UPDATE(updateRequests).reads(request_Q_, req_val, req_rdy).writes(request_D_);
  UPDATE(updateCheck).reads(cycle_Q_, request_Q_, req_val, req_rdy, cmd_val, cmd_bits)
      .reads(cmd_rdy, st_utilization_at_limit, idle, loop_id);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

void Driver::updateDrive() {
  const auto index = static_cast<unsigned>(*request_Q_);
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  req_val = bit(index < tests_.size());
  req_bits = index < tests_.size() ? tests_[index].req : smesh::LoopConvStReq{};
  // Keep Execute incomplete for skipped jobs too; they must finish regardless.
  ex_completed = bit(index < tests_.size() - 1 && cycle >= 4 && cycle % 19 != 13);
  cmd_rdy = bit(!(cycle >= 6 && cycle <= 8) && cycle % 5 != 0);
  st_utilization_at_limit = bit((cycle >= 9 && cycle <= 11) || cycle % 17 == 13);
}

void Driver::updateRequests() {
  if (req_val == 1 && req_rdy == 1) request_D_ = static_cast<unsigned>(*request_Q_) + 1;
}

// Verify every command, stable stalled payloads, and no next-job acceptance
// until the final STORE or post-pooling CONFIG has left the output pipeline.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  passed_ &= req_rdy == idle;
  if (st_utilization_at_limit == 1) { passed_ &= cmd_val == 0; saw_pressure_ = true; }
  if (cmd_val == 1) {
    const auto actual = *cmd_bits;
    if (first_visible_ == 999) first_visible_ = cycle;
    passed_ &= idle == 0 && req_rdy == 0;
    if (stalled_) passed_ &= same(actual, stalled_command_);
    if (cmd_rdy == 0) {
      stalled_ = saw_stall_ = true;
      stalled_command_ = actual;
    } else {
      stalled_ = false;
      unsigned n = seen_;
      const Test* expected = nullptr;
      for (const auto& test : tests_) {
        if (n < test.expected.size()) { expected = &test; break; }
        n -= test.expected.size();
      }
      const bool match = expected && same(actual, expected->expected[n]) && loop_id == expected->req.loop_id;
      if (!match) std::printf("Store command %u mismatch funct=%u rs1=0x%llx rs2=0x%llx\n", seen_, unsigned(actual.funct),
          static_cast<unsigned long long>(actual.rs1), static_cast<unsigned long long>(actual.rs2));
      passed_ &= match;
      ++seen_;
    }
  }
  if (req_val == 1 && req_rdy == 1) {
    unsigned prior = 0;
    for (unsigned i = 0; i < unsigned(*request_Q_); ++i) prior += tests_[i].expected.size();
    passed_ &= seen_ == prior;
  }
}

void Driver::updateCycle() { cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1; }

void Driver::reset() {
  request_D_.reset(0); cycle_D_.reset(0);
  req_val.reset(0); req_bits.reset(smesh::LoopConvStReq{});
  cmd_rdy.reset(0); ex_completed.reset(0); st_utilization_at_limit.reset(0);
  passed_ = true; seen_ = 0; first_visible_ = 999;
  stalled_ = saw_stall_ = saw_pressure_ = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConvSt worker("St");
  worker.req_val << driver.req_val; worker.req_bits << driver.req_bits;
  driver.req_rdy << worker.req_rdy;
  driver.cmd_val << worker.cmd_val; driver.cmd_bits << worker.cmd_bits;
  worker.cmd_rdy << driver.cmd_rdy;
  worker.ex_completed << driver.ex_completed;
  worker.st_utilization_at_limit << driver.st_utilization_at_limit;
  driver.idle << worker.idle; driver.loop_id << worker.loop_id;
  Clock clk; driver.clk << clk; worker.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool passed = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 500; ++cycle) Sim::run();
    passed &= driver.passed();
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_ST] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
