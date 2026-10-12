// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_ld_weight.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConvLdWeight.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

struct Test {
  smesh::LoopConvLdWeightReq request{};
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

// Test-only enumeration of a weight tensor: nested bounded loops, not the
// worker's floorAdd/FSM implementation. Layout strides are checked per command.
Test tensor(unsigned ochs, unsigned kchs, unsigned krows, unsigned kcols,
            bool t1203 = false, bool t0132 = false, bool dw = false) {
  Test test;
  auto& req = test.request;
  req.outer_bounds.kernel_dim = 3;
  req.outer_bounds.in_channels = kchs + 2;
  req.outer_bounds.out_channels = ochs + 2;
  req.outer_bounds.weight_stride = ochs + 3;
  req.inner_bounds.kchs = kchs;
  req.inner_bounds.krows = krows;
  req.inner_bounds.kcols = kcols;
  req.inner_bounds.pochs = ochs;
  req.geometry.ochs = ochs;
  req.geometry.out_channels_per_bank = (ochs + smesh::kDim - 1) / smesh::kDim;
  req.geometry.in_channels_per_bank = (kchs + smesh::kDim - 1) / smesh::kDim;
  req.geometry.weight_spad_stride = krows * kcols * (t0132 ? ochs : kchs);
  req.addr_end = smesh::kSpRows;
  req.dram_addr = 0x80004000;
  req.trans_weight_1203 = bit(t1203);
  req.trans_weight_0132 = bit(t0132);
  req.dw = bit(dw);
  const unsigned max_columns = smesh::kDim * std::max<std::size_t>(1,
      smesh::kDefaultConfig.dma_max_bytes / (smesh::kDim * sizeof(smesh::Elem)));
  const unsigned och_step = t0132 ? smesh::kDim : max_columns;
  const unsigned kch_step = t0132 ? max_columns : smesh::kDim;
  const auto groups = t0132 ? req.geometry.in_channels_per_bank : req.geometry.out_channels_per_bank;
  const auto plane_rows = krows * kcols * (t0132 ? ochs : kchs);
  const std::uint64_t start = req.addr_end - std::uint64_t{groups} * plane_rows;
  const std::uint64_t dram_stride = dw ? 1 : t1203
      ? 9 * req.outer_bounds.out_channels : t0132
      ? req.outer_bounds.in_channels : req.outer_bounds.weight_stride;
  test.expected.push_back(command(0, smesh::packConfigLoadRs1(1, req.geometry.weight_spad_stride,
                                   smesh::kMvinScaleIdentityBits, false, 1), dram_stride * sizeof(smesh::Elem)));
  for (unsigned och = 0; och < ochs; och += och_step)
    for (unsigned kr = 0; kr < krows; ++kr)
      for (unsigned kc = 0; kc < kcols; ++kc)
        for (unsigned ich = 0; ich < kchs; ich += kch_step) {
          const auto& outer = req.outer_bounds;
          const std::uint64_t offset = dw ? kr * 3 + kc : t1203
              ? (ich * 9 + kr * 3 + kc) * outer.out_channels + och : t0132
              ? (kr * 3 * outer.out_channels + kc * outer.out_channels + och) * outer.in_channels + ich
              : (kr * 3 * outer.in_channels + kc * outer.in_channels + ich) * outer.weight_stride + och;
          const auto local = start + (t0132 ? ich : och) / smesh::kDim * plane_rows +
                             (kr * kcols + kc) * (t0132 ? ochs : kchs) + (t0132 ? och : ich);
          const auto rows = std::min<unsigned>(smesh::kDim, t0132 ? ochs - och : kchs - ich);
          const auto cols = std::min<unsigned>(max_columns, t0132 ? kchs - ich : ochs - och);
          test.expected.push_back(command(1, req.dram_addr + offset * sizeof(smesh::Elem),
              smesh::packLocal(smesh::makeSpAddr(static_cast<std::uint32_t>(local)), {rows, cols})));
        }
  return test;
}

std::vector<Test> cases() {
  std::vector<Test> tests;
  tests.push_back(tensor(5, 5, 1, 1));
  // Literal anchors independently check load-state encoding and address/shape
  // packing for the default four-element row and 32-row SPAD configuration.
  if (smesh::kDim == 4 && smesh::kSpRows == 32 && sizeof(smesh::Elem) == 1) {
    tests[0].expected = {
        command(0, 0x3f80000000050109ull, 8),
        command(1, 0x80004000, 0x0004000500000016ull),
        command(1, 0x80004020, 0x000100050000001aull),
    };
  }
  tests.push_back(tensor(3, 2, 2, 2));
  tests.push_back(tensor(3, 5, 2, 1, true));
  tests.push_back(tensor(5, 5, 1, 1, false, true));
  tests.push_back(tensor(3, 5, 2, 2, false, true));
  tests.push_back(tensor(1, 1, 2, 2, false, false, true));
  const unsigned wide = smesh::kDefaultConfig.dma_max_bytes / sizeof(smesh::Elem) + 1;
  tests.push_back(tensor(wide, 1, 1, 1));
  tests.push_back(tensor(1, wide, 1, 1, false, true));
  // Arithmetic-only option overlap: dw wins DRAM selection; 0132 controls SPAD.
  tests.push_back(tensor(2, 3, 1, 1, true, true, true));
  Test high_offset = tensor(1, 5, 1, 1, true);
  high_offset.request.outer_bounds.kernel_dim = 65535;
  high_offset.request.outer_bounds.out_channels = 60000;
  const auto stride = 65535ull * 65535 * 60000 * sizeof(smesh::Elem);
  high_offset.expected[0].rs2 = stride;
  high_offset.expected[2].rs1 = high_offset.request.dram_addr + ((4 * stride) & 0xffffffffull);
  tests.push_back(high_offset);
  tests.push_back(Test{}); // Zero source skips the whole job, including CONFIG.
  for (std::size_t i = 0; i < tests.size(); ++i) tests[i].request.loop_id = i % 2;
  return tests;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::LoopConvLdWeightReq, req_bits);
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
    return passed_ && *request_Q_ == tests_.size() && seen_ == total_ && idle == 1 &&
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
  req_val = bit(index < tests_.size());
  req_bits = index < tests_.size() ? tests_[index].request : smesh::LoopConvLdWeightReq{};
  wait_for_prev_loop = bit((cycle >= 1 && cycle <= 3) || (cycle >= 20 && cycle <= 23));
  cmd_rdy = bit(!(cycle >= 6 && cycle <= 8) && cycle % 5 != 0);
  ld_utilization_at_limit = bit((cycle >= 9 && cycle <= 11) || cycle % 17 == 13);
}

void Driver::updateRequests() {
  if (req_val == 1 && req_rdy == 1) request_D_ = static_cast<unsigned>(*request_Q_) + 1;
}

// Check every transfer, output stability during stalls, and job acceptance only
// after the previous job's final command has left both pipeline stages.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  passed_ &= req_rdy == idle;
  if (ld_utilization_at_limit == 1) {
    passed_ &= cmd_val == 0;
    saw_pressure_ = true;
  }
  if (cmd_val == 1) {
    if (first_visible_ == 999) first_visible_ = cycle;
    const auto actual = *cmd_bits;
    passed_ &= idle == 0 && req_rdy == 0;
    if (stalled_) passed_ &= same(actual, stalled_command_);
    if (cmd_rdy == 0) {
      stalled_ = true;
      stalled_command_ = actual;
      saw_stall_ = true;
    } else {
      stalled_ = false;
      unsigned n = seen_;
      const Test* expected = nullptr;
      for (const auto& test : tests_) {
        if (n < test.expected.size()) { expected = &test; break; }
        n -= test.expected.size();
      }
      const bool match = expected && same(actual, expected->expected[n]) && loop_id == expected->request.loop_id;
      if (!match) std::printf("Weight command %u mismatch funct=%u rs1=0x%llx rs2=0x%llx\n",
                             seen_, static_cast<unsigned>(actual.funct),
                             static_cast<unsigned long long>(actual.rs1),
                             static_cast<unsigned long long>(actual.rs2));
      passed_ &= match;
      ++seen_;
    }
  }
  if (req_val == 1 && req_rdy == 1) {
    const auto index = static_cast<unsigned>(*request_Q_);
    unsigned prior = 0;
    for (unsigned i = 0; i < index; ++i) prior += tests_[i].expected.size();
    passed_ &= seen_ == prior;
  }
}

void Driver::updateCycle() { cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1; }

void Driver::reset() {
  request_D_.reset(0);
  cycle_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::LoopConvLdWeightReq{});
  cmd_rdy.reset(0);
  ld_utilization_at_limit.reset(0);
  wait_for_prev_loop.reset(0);
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
  smesh::LoopConvLdWeight worker("LdWeight");
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
  bool passed = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 300; ++cycle) Sim::run();
    passed &= driver.passed();
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_LD_WEIGHT] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
