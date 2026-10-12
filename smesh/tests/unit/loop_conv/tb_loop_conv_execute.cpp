// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_execute.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "LoopConvExecute.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

struct Test {
  smesh::LoopConvExecuteReq req{};
  std::vector<smesh::SmeshCmd> expected;
};

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

// Reference tensor enumeration uses nested loops, not the worker's FSM or
// wrapping-counter implementation. Each weight group retains weights across
// its output positions; skipped positions do not consume that first-use flag.
Test reference(smesh::LoopConvExecuteReq req) {
  Test test{req, {}};
  const auto& inner = req.inner_bounds;
  const auto& outer = req.outer_bounds;
  const bool ti = req.trans_input_3120 == 1;
  const bool tw = req.trans_weight_0132 == 1;
  const unsigned dil = req.input_dilated == 1;
  const unsigned ds = req.downsample == 1;
  const auto undilated = [dil](unsigned n) { return (n + dil) >> dil; };
  if (ti) test.expected.push_back(command(0,
      smesh::packConfigExRs1(req.input_geometry.irows * req.input_geometry.icols, false, false, 0, true, 0, 0),
      smesh::packConfigExRs2(inner.orows * inner.ocols)));
  const auto weight_channels = tw ? req.weight_geometry.ochs : inner.kchs;
  const auto plane = inner.krows * inner.kcols * weight_channels;
  const auto groups = tw ? req.weight_geometry.in_channels_per_bank : req.weight_geometry.out_channels_per_bank;
  const std::uint64_t b_start = req.b_addr_end - std::uint64_t{groups} * plane;
  for (unsigned och = 0; och < req.weight_geometry.ochs; och += smesh::kDim)
    for (unsigned kr = 0; kr < inner.krows; ++kr)
      for (unsigned kc = 0; kc < inner.kcols; kc += req.max_pixels_per_row)
        for (unsigned ch = 0; ch < inner.kchs; ch += smesh::kDim) {
          bool first = true;
          const auto rot_row = req.wrot180 == 1 ? inner.krows - kr - 1 : kr;
          const auto rot_col = req.wrot180 == 1 ? inner.kcols - kc - 1 : kc;
          const auto b = b_start + (tw ? ch : och) / smesh::kDim * plane +
              (rot_row * inner.kcols + rot_col) * weight_channels + (tw ? och : ch);
          for (unsigned batch = 0; batch < inner.batches; batch += ti ? smesh::kDim : 1)
            for (unsigned row = 0; row < inner.orows; ++row)
              for (unsigned col = 0; col < inner.ocols;) {
                const bool skip = dil && (((kr * outer.kernel_dilation + row - inner.upad) & 1) ||
                                         ((kc * outer.kernel_dilation + col - inner.lpad) & 1));
                if (skip) { ++col; continue; }
                const auto i = ti ? std::min<unsigned>(smesh::kDim, inner.batches - batch) :
                    undilated(std::min<unsigned>(smesh::kDim << dil, inner.ocols - col));
                const auto j = std::min<unsigned>(smesh::kDim, req.weight_geometry.ochs - och);
                const auto k = std::min<unsigned>(req.max_pixels_per_row, inner.kcols - kc) *
                               std::min<unsigned>(smesh::kDim, inner.kchs - ch);
                const auto in_row = undilated(row * outer.stride + kr * outer.kernel_dilation);
                const auto in_col = undilated(col * outer.stride + kc * outer.kernel_dilation);
                const auto rows = req.input_geometry.irows >> ds;
                const auto cols = req.input_geometry.icols >> ds;
                const auto a = req.a_addr_start + (ti ? batch : ch) / smesh::kDim * req.input_geometry.input_spad_stride +
                    (ti ? ch : batch) * rows * cols + (in_row >> ds) * cols + (in_col >> ds);
                const auto c = req.c_addr_start + (och / smesh::kDim * inner.batches + batch) *
                               inner.orows * inner.ocols + row * inner.ocols + col;
                test.expected.push_back(command(6,
                    smesh::packLocal(first ? smesh::makeSpAddr(static_cast<std::uint32_t>(b)).raw : 0xffffffffu, {k, j}),
                    smesh::packLocal(smesh::makeAccAddr(c, true), {i, j})));
                test.expected.push_back(command(first ? 4 : 5,
                    smesh::packLocal(smesh::makeSpAddr(a), {i, k}), smesh::packLocal(0xffffffffu, {i, j})));
                first = false;
                col += ti ? 1 : smesh::kDim << dil;
              }
        }
  return test;
}

// Fill the same derived request fields the scheduler would provide.
void geometry(smesh::LoopConvExecuteReq& req) {
  const auto& inner = req.inner_bounds;
  const auto& outer = req.outer_bounds;
  const bool dil = req.input_dilated == 1;
  const auto extent = [&](unsigned n, unsigned kernel, unsigned before, unsigned after) {
    const auto padded = n * outer.stride + kernel + (outer.kernel_dilation - 1) * (kernel - 1) - 1;
    const auto unpadded = (padded - before - after + unsigned(dil)) >> unsigned(dil);
    return dil ? unpadded + ((before + 1) >> 1) + ((after + 1) >> 1) : padded;
  };
  auto& input = req.input_geometry;
  input.irows = extent(inner.orows, inner.krows, inner.upad, inner.dpad);
  input.icols = extent(inner.ocols, inner.kcols, inner.lpad, inner.rpad);
  input.ichs = inner.kchs;
  const unsigned ds = req.downsample == 1;
  input.input_spad_stride = (req.trans_input_3120 == 1 ? inner.kchs : inner.batches) *
                            (input.irows >> ds) * (input.icols >> ds);
  auto& weight = req.weight_geometry;
  weight.ochs = inner.pochs;
  weight.out_channels_per_bank = (inner.pochs + smesh::kDim - 1) / smesh::kDim;
  weight.in_channels_per_bank = (inner.kchs + smesh::kDim - 1) / smesh::kDim;
  weight.weight_spad_stride = inner.krows * inner.kcols * (req.trans_weight_0132 == 1 ? inner.pochs : inner.kchs);
}

std::vector<Test> cases() {
  smesh::LoopConvExecuteReq base{};
  base.outer_bounds.stride = base.outer_bounds.kernel_dilation = 1;
  base.inner_bounds.batches = base.inner_bounds.orows = base.inner_bounds.krows = base.inner_bounds.kcols = 1;
  base.inner_bounds.ocols = 5;
  base.inner_bounds.kchs = 2;
  base.inner_bounds.pochs = 3;
  base.b_addr_end = smesh::kSpRows / 2;
  base.max_pixels_per_row = 1;
  geometry(base);
  std::vector<Test> tests{reference(base)};
  if (smesh::kDim == 4 && smesh::kSpRows == 32) {
    // Independent literal anchors: real weights/FLIP, then garbage weights/STAY.
    tests[0].expected = {
      command(6, 0x000200030000000eull, 0x00040003c0000000ull),
      command(4, 0x0004000200000000ull, 0x00040003ffffffffull),
      command(6, 0x00020003ffffffffull, 0x00010003c0000004ull),
      command(5, 0x0001000200000004ull, 0x00010003ffffffffull),
    };
  }
  auto req = base;
  req.inner_bounds.batches = req.inner_bounds.orows = 2;
  req.inner_bounds.kchs = req.inner_bounds.pochs = 5;
  req.c_addr_start = smesh::kAccRows / 2;
  geometry(req); tests.push_back(reference(req)); // all spatial/batch/channel wraps
  req = base;
  req.inner_bounds.ocols = 1;
  req.inner_bounds.krows = req.inner_bounds.kcols = 2;
  req.outer_bounds.kernel_dilation = 2;
  req.wrot180 = 1;
  geometry(req); tests.push_back(reference(req));
  req = base;
  req.trans_input_3120 = req.trans_weight_0132 = 1;
  req.inner_bounds.batches = 5;
  req.inner_bounds.ocols = 2;
  req.inner_bounds.kchs = req.inner_bounds.pochs = 5;
  geometry(req); tests.push_back(reference(req));
  req = base;
  req.downsample = 1;
  req.outer_bounds.stride = 2;
  req.inner_bounds.ocols = 2;
  geometry(req); tests.push_back(reference(req));
  req = base;
  req.inner_bounds.kchs = 1;
  req.inner_bounds.kcols = 3;
  req.max_pixels_per_row = 2;
  geometry(req); tests.push_back(reference(req)); // grouped pixels and final pixel tail
  req = base;
  req.input_dilated = 1;
  req.inner_bounds.lpad = 1;
  geometry(req); tests.push_back(reference(req)); // skip first position; still FLIP
  req.inner_bounds.lpad = 0;
  req.inner_bounds.orows = 2;
  req.inner_bounds.krows = 2;
  geometry(req); tests.push_back(reference(req)); // parity skips in both dimensions
  req = base;
  req.input_dilated = 1;
  req.inner_bounds.upad = 1;
  geometry(req); tests.push_back(reference(req)); // every position skipped, no commands
  for (unsigned i = 0; i < tests.size(); ++i) tests[i].req.loop_id = i % 2;
  return tests;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::LoopConvExecuteReq, req_bits);
  Input(bit, req_rdy);
  Output(bit, cmd_rdy);
  Input(bit, cmd_val);
  Input(smesh::SmeshCmd, cmd_bits);
  Output(bit, lda_completed);
  Output(bit, ldb_completed);
  Output(bit, ldd_completed);
  Output(bit, ex_utilization_at_limit);
  Input(bit, idle);
  Input(u8, loop_id);
  void updateDrive();
  void updateRequests();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const {
    return passed_ && request_Q_ == tests_.size() && seen_ == expected_count_ && idle == 1 &&
           saw_stall_ && saw_pressure_ && first_visible_ == 6;
  }
 private:
  const std::vector<Test> tests_ = cases();
  unsigned expected_count_ = 0;
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
  for (const auto& test : tests_) expected_count_ += test.expected.size();
  request_Q_ <= request_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(request_Q_, cycle_Q_)
      .writes(req_val, req_bits, cmd_rdy, lda_completed, ldb_completed, ldd_completed, ex_utilization_at_limit);
  UPDATE(updateRequests).reads(request_Q_, req_val, req_rdy).writes(request_D_);
  UPDATE(updateCheck).reads(cycle_Q_, request_Q_, req_val, req_rdy, cmd_val, cmd_bits)
      .reads(cmd_rdy, ex_utilization_at_limit, idle, loop_id);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

void Driver::updateDrive() {
  const auto index = static_cast<unsigned>(*request_Q_);
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  req_val = bit(index < tests_.size());
  req_bits = index < tests_.size() ? tests_[index].req : smesh::LoopConvExecuteReq{};
  // Individually block generation on each load input. Periodic later drops
  // exercise already-buffered commands draining while new generation waits.
  const bool all_skipped_job = index == tests_.size();
  lda_completed = bit(!all_skipped_job && cycle != 1 && cycle % 23 != 17);
  ldb_completed = bit(!all_skipped_job && cycle != 2 && cycle % 23 != 18);
  ldd_completed = bit(!all_skipped_job && cycle != 3 && cycle % 23 != 19);
  cmd_rdy = bit(!(cycle >= 6 && cycle <= 8) && cycle % 5 != 0);
  ex_utilization_at_limit = bit((cycle >= 9 && cycle <= 11) || cycle % 17 == 13);
}

void Driver::updateRequests() {
  if (req_val == 1 && req_rdy == 1) request_D_ = static_cast<unsigned>(*request_Q_) + 1;
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  passed_ &= req_rdy == idle;
  if (ex_utilization_at_limit == 1) { passed_ &= cmd_val == 0; saw_pressure_ = true; }
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
      if (!match) std::printf("Execute command %u mismatch funct=%u rs1=0x%llx rs2=0x%llx\n",
          seen_, unsigned(actual.funct), static_cast<unsigned long long>(actual.rs1), static_cast<unsigned long long>(actual.rs2));
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
  request_D_.reset(0);
  cycle_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::LoopConvExecuteReq{});
  cmd_rdy.reset(0);
  lda_completed.reset(0);
  ldb_completed.reset(0);
  ldd_completed.reset(0);
  ex_utilization_at_limit.reset(0);
  passed_ = true;
  seen_ = 0;
  first_visible_ = 999;
  stalled_ = saw_stall_ = saw_pressure_ = false;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConvExecute worker("Execute");
  worker.req_val << driver.req_val;
  worker.req_bits << driver.req_bits;
  driver.req_rdy << worker.req_rdy;
  driver.cmd_val << worker.cmd_val;
  driver.cmd_bits << worker.cmd_bits;
  worker.cmd_rdy << driver.cmd_rdy;
  worker.lda_completed << driver.lda_completed;
  worker.ldb_completed << driver.ldb_completed;
  worker.ldd_completed << driver.ldd_completed;
  worker.ex_utilization_at_limit << driver.ex_utilization_at_limit;
  driver.idle << worker.idle;
  driver.loop_id << worker.loop_id;
  Clock clk;
  driver.clk << clk;
  worker.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool passed = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 1200; ++cycle) Sim::run();
    passed &= driver.passed();
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_EXECUTE] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
