// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_cmd_dec.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConvCmdDec.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

struct Test {
  bit valid = 1;
  smesh::SmeshQueuedCmd command{};
  smesh::LoopConvDecodedCmd expected{};
};

std::vector<Test> testCases() {
  std::vector<Test> tests;
  const auto add = [&](smesh::SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2,
                       smesh::LoopConvDecodedCmd expected) {
    Test item{};
    item.command.cmd.funct = static_cast<std::uint32_t>(funct);
    item.command.cmd.rs1 = rs1;
    item.command.cmd.rs2 = rs2;
    item.expected = expected;
    tests.push_back(item);
    // An invalid head carrying the same payload must clear every decoder output.
    item.valid = 0;
    item.expected = {};
    tests.push_back(item);
  };

  smesh::LoopConvDecodedCmd expected{};
  // Literal expected values follow Original's config slices, including config 4's 11/10-bit fields.
  expected = {};
  expected.do_config = 1;
  expected.config_id = 1;
  expected.outer_bounds.out_channels = 0x1234;
  expected.outer_bounds.in_channels = 0x5678;
  expected.outer_bounds.in_row_dim = 0x9abc;
  expected.outer_bounds.batch_size = 0xdef0;
  expected.outer_bounds.padding = 0xf;
  expected.outer_bounds.stride = 0xed;
  expected.outer_bounds.out_col_dim = 0xcba9;
  expected.outer_bounds.pool_out_row_dim = 0x8765;
  expected.outer_bounds.out_row_dim = 0x4321;
  add(smesh::SmeshFunct::LoopConvWsConfig1, 0x123456789abcdef0ull,
      0x0fedcba987654321ull, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 2;
  expected.outer_bounds.kernel_dim = 0x1234;
  expected.outer_bounds.pool_out_col_dim = 0x5678;
  expected.outer_bounds.pool_size = 0x9abc;
  expected.outer_bounds.pool_stride = 0xde;
  expected.outer_bounds.pool_padding = 0xf0;
  expected.inner_bounds.batches = 0xfed;
  expected.inner_bounds.porows = 0xcba9;
  expected.inner_bounds.pocols = 0x8765;
  expected.inner_bounds.pochs = 0x4321;
  add(smesh::SmeshFunct::LoopConvWsConfig2, 0x123456789abcdef0ull,
      0x0fedcba987654321ull, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 3;
  expected.inner_bounds.krows = 0x1234;
  expected.inner_bounds.kcols = 0x5678;
  expected.inner_bounds.kchs = 0x9abc;
  expected.inner_bounds.lpad = 0xdef0;
  expected.inner_bounds.rpad = 0xfed;
  expected.inner_bounds.upad = 0xcba9;
  expected.inner_bounds.dpad = 0x87;
  expected.inner_bounds.plpad = 0x65;
  expected.outer_bounds.in_col_dim = 0x4321;
  add(smesh::SmeshFunct::LoopConvWsConfig3, 0x123456789abcdef0ull,
      0x0fedcba987654321ull, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 4;
  expected.inner_bounds.orows = 0x1234;
  expected.inner_bounds.prad = 0x5678;
  expected.inner_bounds.pupad = 0x4d5;
  expected.inner_bounds.pdpad = 0x737;
  expected.outer_bounds.kernel_dilation = 0x2f0;
  expected.inner_bounds.ocols = 0x4321;
  expected.outer_bounds.in_stride = 0xfed;
  expected.outer_bounds.weight_stride = 0xcba9;
  expected.outer_bounds.out_stride = 0x8765;
  add(smesh::SmeshFunct::LoopConvWsConfig4, 0x123456789abcdef0ull,
      0x0fedcba987654321ull, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 5;
  expected.weights_dram_addr = 0xfedcba9876543210ull;
  expected.output_dram_addr = 0x8123456789abcdefull;
  add(smesh::SmeshFunct::LoopConvWsConfig5, expected.weights_dram_addr,
      expected.output_dram_addr, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 6;
  expected.bias_dram_addr = 0x87654321fedcba98ull;
  expected.input_dram_addr = 0xf123456789abcdefull;
  add(smesh::SmeshFunct::LoopConvWsConfig6, expected.bias_dram_addr,
      expected.input_dram_addr, expected);

  // Maximum raw field values must survive extraction without signed conversion.
  expected = {};
  expected.do_config = 1;
  expected.config_id = 1;
  expected.outer_bounds.out_channels = 0xffff;
  expected.outer_bounds.in_channels = 0xffff;
  expected.outer_bounds.in_row_dim = 0xffff;
  expected.outer_bounds.batch_size = 0xffff;
  expected.outer_bounds.padding = 0xff;
  expected.outer_bounds.stride = 0xff;
  expected.outer_bounds.out_col_dim = 0xffff;
  expected.outer_bounds.pool_out_row_dim = 0xffff;
  expected.outer_bounds.out_row_dim = 0xffff;
  add(smesh::SmeshFunct::LoopConvWsConfig1, ~std::uint64_t{0}, ~std::uint64_t{0}, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 2;
  expected.outer_bounds.kernel_dim = 0xffff;
  expected.outer_bounds.pool_out_col_dim = 0xffff;
  expected.outer_bounds.pool_size = 0xffff;
  expected.outer_bounds.pool_stride = 0xff;
  expected.outer_bounds.pool_padding = 0xff;
  expected.inner_bounds.batches = 0xffff;
  expected.inner_bounds.porows = 0xffff;
  expected.inner_bounds.pocols = 0xffff;
  expected.inner_bounds.pochs = 0xffff;
  add(smesh::SmeshFunct::LoopConvWsConfig2, ~std::uint64_t{0}, ~std::uint64_t{0}, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 3;
  expected.inner_bounds.krows = 0xffff;
  expected.inner_bounds.kcols = 0xffff;
  expected.inner_bounds.kchs = 0xffff;
  expected.inner_bounds.lpad = 0xffff;
  expected.inner_bounds.rpad = 0xffff;
  expected.inner_bounds.upad = 0xffff;
  expected.inner_bounds.dpad = 0xff;
  expected.inner_bounds.plpad = 0xff;
  expected.outer_bounds.in_col_dim = 0xffff;
  add(smesh::SmeshFunct::LoopConvWsConfig3, ~std::uint64_t{0}, ~std::uint64_t{0}, expected);

  expected = {};
  expected.do_config = 1;
  expected.config_id = 4;
  expected.inner_bounds.orows = 0xffff;
  expected.inner_bounds.prad = 0xffff;
  expected.inner_bounds.pupad = 0x7ff;
  expected.inner_bounds.pdpad = 0x7ff;
  expected.outer_bounds.kernel_dilation = 0x3ff;
  expected.inner_bounds.ocols = 0xffff;
  expected.outer_bounds.in_stride = 0xffff;
  expected.outer_bounds.weight_stride = 0xffff;
  expected.outer_bounds.out_stride = 0xffff;
  add(smesh::SmeshFunct::LoopConvWsConfig4, ~std::uint64_t{0}, ~std::uint64_t{0}, expected);

  expected = {};
  expected.do_launch = 1;
  add(smesh::SmeshFunct::LoopConvWs, 0, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.no_bias = 1;
  add(smesh::SmeshFunct::LoopConvWs, std::uint64_t{1} << 0, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.wrot180 = 1;
  add(smesh::SmeshFunct::LoopConvWs, std::uint64_t{1} << 1, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.trans_output_1203 = 1;
  add(smesh::SmeshFunct::LoopConvWs, std::uint64_t{1} << 2, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.trans_weight_1203 = 1;
  add(smesh::SmeshFunct::LoopConvWs, std::uint64_t{1} << 3, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.trans_weight_0132 = 1;
  add(smesh::SmeshFunct::LoopConvWs, std::uint64_t{1} << 4, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.trans_input_3120 = 1;
  add(smesh::SmeshFunct::LoopConvWs, std::uint64_t{1} << 5, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.dw = 1;
  add(smesh::SmeshFunct::LoopConvWs, std::uint64_t{1} << 6, 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.no_pool = 1;
  add(smesh::SmeshFunct::LoopConvWs, 0, std::uint64_t{1} << 0, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.downsample = 1;
  add(smesh::SmeshFunct::LoopConvWs, 0, std::uint64_t{1} << 1, expected);
  expected = {};
  expected.do_launch = 1;
  expected.launch.input_dilated = 1;
  add(smesh::SmeshFunct::LoopConvWs, 0, std::uint64_t{1} << 2, expected);

  expected = {};
  expected.do_launch = 1;
  expected.launch.no_bias = 1;
  expected.launch.wrot180 = 1;
  expected.launch.trans_output_1203 = 1;
  expected.launch.trans_weight_1203 = 1;
  expected.launch.trans_weight_0132 = 1;
  expected.launch.trans_input_3120 = 1;
  expected.launch.dw = 1;
  expected.launch.no_pool = 1;
  expected.launch.downsample = 1;
  expected.launch.input_dilated = 1;
  expected.launch.activation = 3;
  expected.launch.max_pixels_per_row = 255;
  expected.launch.a_ex_spad_id = 3;
  expected.launch.b_ex_spad_id = 3;
  add(smesh::SmeshFunct::LoopConvWs, ~std::uint64_t{0}, ~std::uint64_t{0}, expected);

  // Distinct launch fields check adjacency and A/B identifier ordering.
  expected = {};
  expected.do_launch = 1;
  expected.launch.max_pixels_per_row = 0x5a;
  expected.launch.a_ex_spad_id = 2;
  expected.launch.b_ex_spad_id = 1;
  expected.launch.activation = 2;
  add(smesh::SmeshFunct::LoopConvWs, 0x95a00, 0x10, expected);

  // Ordinary commands yield an empty packet even with nonzero operand bits.
  add(smesh::SmeshFunct::Mvin3, ~std::uint64_t{0}, ~std::uint64_t{0}, {});
  add(static_cast<smesh::SmeshFunct>(22), ~std::uint64_t{0}, ~std::uint64_t{0}, {});
  add(smesh::SmeshFunct::StoreSpad, ~std::uint64_t{0}, ~std::uint64_t{0}, {});
  return tests;
}

bool compare(const smesh::LoopConvDecodedCmd& actual,
             const smesh::LoopConvDecodedCmd& expected, unsigned cycle) {
  bool passed = true;
#define CHECK(member) \
  if (actual.member != expected.member) { \
    std::printf("Decoder cycle=%u field=%s actual=%llu expected=%llu\n", cycle, #member, \
                static_cast<unsigned long long>(actual.member), \
                static_cast<unsigned long long>(expected.member)); \
    passed = false; \
  }
  CHECK(do_config);
  CHECK(do_launch);
  CHECK(config_id);
  CHECK(outer_bounds.batch_size);
  CHECK(outer_bounds.in_row_dim);
  CHECK(outer_bounds.in_col_dim);
  CHECK(outer_bounds.in_channels);
  CHECK(outer_bounds.out_channels);
  CHECK(outer_bounds.out_col_dim);
  CHECK(outer_bounds.out_row_dim);
  CHECK(outer_bounds.out_stride);
  CHECK(outer_bounds.in_stride);
  CHECK(outer_bounds.weight_stride);
  CHECK(outer_bounds.pool_out_row_dim);
  CHECK(outer_bounds.pool_out_col_dim);
  CHECK(outer_bounds.stride);
  CHECK(outer_bounds.padding);
  CHECK(outer_bounds.kernel_dim);
  CHECK(outer_bounds.kernel_dilation);
  CHECK(outer_bounds.pool_size);
  CHECK(outer_bounds.pool_stride);
  CHECK(outer_bounds.pool_padding);
  CHECK(inner_bounds.batches);
  CHECK(inner_bounds.porows);
  CHECK(inner_bounds.pocols);
  CHECK(inner_bounds.pochs);
  CHECK(inner_bounds.krows);
  CHECK(inner_bounds.kcols);
  CHECK(inner_bounds.kchs);
  CHECK(inner_bounds.lpad);
  CHECK(inner_bounds.rpad);
  CHECK(inner_bounds.upad);
  CHECK(inner_bounds.dpad);
  CHECK(inner_bounds.plpad);
  CHECK(inner_bounds.prad);
  CHECK(inner_bounds.pupad);
  CHECK(inner_bounds.pdpad);
  CHECK(inner_bounds.orows);
  CHECK(inner_bounds.ocols);
  CHECK(launch.no_bias);
  CHECK(launch.wrot180);
  CHECK(launch.trans_output_1203);
  CHECK(launch.trans_weight_1203);
  CHECK(launch.trans_weight_0132);
  CHECK(launch.trans_input_3120);
  CHECK(launch.dw);
  CHECK(launch.no_pool);
  CHECK(launch.downsample);
  CHECK(launch.input_dilated);
  CHECK(launch.activation);
  CHECK(launch.max_pixels_per_row);
  CHECK(launch.a_ex_spad_id);
  CHECK(launch.b_ex_spad_id);
  CHECK(weights_dram_addr);
  CHECK(output_dram_addr);
  CHECK(bias_dram_addr);
  CHECK(input_dram_addr);
#undef CHECK
  return passed;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, head_val);
  Output(smesh::SmeshQueuedCmd, head_bits);
  Input(smesh::LoopConvDecodedCmd, decoded);

  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  std::size_t cycles() const { return tests_.size(); }
  bool passed() const {
    return passed_ && std::all_of(observed_.begin(), observed_.end(), [](bool seen) { return seen; });
  }

 private:
  const std::vector<Test> tests_ = testCases();
  std::vector<bool> observed_ = std::vector<bool>(tests_.size(), false);
  bool passed_ = true;
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(head_val, head_bits);
  UPDATE(updateCheck).reads(cycle_Q_, decoded);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  head_val = cycle < tests_.size() ? tests_[cycle].valid : bit(0);
  head_bits = cycle < tests_.size() ? tests_[cycle].command : smesh::SmeshQueuedCmd{};
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= tests_.size()) return;
  passed_ &= compare(*decoded, tests_[cycle].expected, cycle);
  observed_[cycle] = true;
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void Driver::reset() {
  cycle_D_.reset(0);
  head_val.reset(0);
  head_bits.reset(smesh::SmeshQueuedCmd{});
  std::fill(observed_.begin(), observed_.end(), false);
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  smesh::LoopConvCmdDec decoder("CmdDec");
  decoder.head_val << driver.head_val;
  decoder.head_bits << driver.head_bits;
  driver.decoded << decoder.decoded;
  Clock clk;
  driver.clk << clk;
  decoder.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (std::size_t cycle = 0; cycle < driver.cycles(); ++cycle) Sim::run();

  const bool passed = driver.passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_CMD_DEC] %s (%zu cases)\n", passed ? "PASS" : "FAIL", driver.cycles());
  return passed ? 0 : 1;
}
