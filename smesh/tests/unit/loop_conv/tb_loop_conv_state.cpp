// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_state.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConv.hpp"
#include "SmeshCommand.hpp"
#include "SmeshConfig.hpp"
#include "SmeshTypes.hpp"

#include <array>
#include <cstdio>

namespace {

smesh::SmeshQueuedCmd command(unsigned funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = funct;
  value.cmd.rs1 = rs1;
  value.cmd.rs2 = rs2;
  return value;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::SmeshQueuedCmd, in_bits);
  Input(bit, in_rdy);
  Output(bit, out_rdy);
  Input(bit, out_val);
  Input(bit, busy);
  Input(smesh::LoopConvSlot, loop0);
  Input(smesh::LoopConvSlot, loop1);
  Input(u8, head_loop_id);
  Input(u8, tail_loop_id);
  Input(u8, config_loop_id);

  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const { return passed_ && observed_ == 0xfffffu; }

 private:
  std::array<smesh::SmeshQueuedCmd, 15> program_{};
  std::array<smesh::LoopConvSlot, 2> expected_{};
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
  bool passed_ = true;
  unsigned observed_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  // Six different commands must accumulate into one saved setup per slot.
  expected_[0].outer_bounds.out_channels = 1;
  expected_[0].outer_bounds.in_channels = 2;
  expected_[0].outer_bounds.in_row_dim = 3;
  expected_[0].outer_bounds.batch_size = 4;
  expected_[0].outer_bounds.padding = 5;
  expected_[0].outer_bounds.stride = 6;
  expected_[0].outer_bounds.out_col_dim = 7;
  expected_[0].outer_bounds.pool_out_row_dim = 8;
  expected_[0].outer_bounds.out_row_dim = 9;
  program_[0] = command(16, 0x1000200030004ull, 0x506000700080009ull);
  expected_[0].outer_bounds.kernel_dim = 10;
  expected_[0].outer_bounds.pool_out_col_dim = 11;
  expected_[0].outer_bounds.pool_size = 12;
  expected_[0].outer_bounds.pool_stride = 13;
  expected_[0].outer_bounds.pool_padding = 14;
  expected_[0].inner_bounds.batches = 15;
  expected_[0].inner_bounds.porows = 16;
  expected_[0].inner_bounds.pocols = 17;
  expected_[0].inner_bounds.pochs = 18;
  program_[1] = command(17, 0xa000b000c0d0eull, 0xf001000110012ull);
  expected_[0].inner_bounds.krows = 19;
  expected_[0].inner_bounds.kcols = 20;
  expected_[0].inner_bounds.kchs = 21;
  expected_[0].inner_bounds.lpad = 22;
  expected_[0].inner_bounds.rpad = 23;
  expected_[0].inner_bounds.upad = 24;
  expected_[0].inner_bounds.dpad = 25;
  expected_[0].inner_bounds.plpad = 26;
  expected_[0].outer_bounds.in_col_dim = 27;
  program_[2] = command(18, 0x13001400150016ull, 0x170018191a001bull);
  expected_[0].inner_bounds.orows = 28;
  expected_[0].inner_bounds.prad = 29;
  expected_[0].inner_bounds.pupad = 30;
  expected_[0].inner_bounds.pdpad = 31;
  expected_[0].outer_bounds.kernel_dilation = 32;
  expected_[0].inner_bounds.ocols = 33;
  expected_[0].outer_bounds.in_stride = 34;
  expected_[0].outer_bounds.weight_stride = 35;
  expected_[0].outer_bounds.out_stride = 36;
  program_[3] = command(19, 0x1c001d03c07c20ull, 0x22002300240021ull);
  expected_[0].weights_dram_addr = 0xa000123456789abcull;
  expected_[0].output_dram_addr = 0xa00023456789abcdull;
  expected_[0].bias_dram_addr = 0xa0003456789abcdeull;
  expected_[0].input_dram_addr = 0xa000456789abcdefull;
  program_[4] = command(20, expected_[0].weights_dram_addr, expected_[0].output_dram_addr);
  program_[5] = command(21, expected_[0].bias_dram_addr, expected_[0].input_dram_addr);

  expected_[1].outer_bounds.out_channels = 51;
  expected_[1].outer_bounds.in_channels = 52;
  expected_[1].outer_bounds.in_row_dim = 53;
  expected_[1].outer_bounds.batch_size = 54;
  expected_[1].outer_bounds.padding = 55;
  expected_[1].outer_bounds.stride = 56;
  expected_[1].outer_bounds.out_col_dim = 57;
  expected_[1].outer_bounds.pool_out_row_dim = 58;
  expected_[1].outer_bounds.out_row_dim = 59;
  program_[7] = command(16, 0x33003400350036ull, 0x37380039003a003bull);
  expected_[1].outer_bounds.kernel_dim = 60;
  expected_[1].outer_bounds.pool_out_col_dim = 61;
  expected_[1].outer_bounds.pool_size = 62;
  expected_[1].outer_bounds.pool_stride = 63;
  expected_[1].outer_bounds.pool_padding = 64;
  expected_[1].inner_bounds.batches = 65;
  expected_[1].inner_bounds.porows = 66;
  expected_[1].inner_bounds.pocols = 67;
  expected_[1].inner_bounds.pochs = 68;
  program_[8] = command(17, 0x3c003d003e3f40ull, 0x41004200430044ull);
  expected_[1].inner_bounds.krows = 69;
  expected_[1].inner_bounds.kcols = 70;
  expected_[1].inner_bounds.kchs = 71;
  expected_[1].inner_bounds.lpad = 72;
  expected_[1].inner_bounds.rpad = 73;
  expected_[1].inner_bounds.upad = 74;
  expected_[1].inner_bounds.dpad = 75;
  expected_[1].inner_bounds.plpad = 76;
  expected_[1].outer_bounds.in_col_dim = 77;
  program_[9] = command(18, 0x45004600470048ull, 0x49004a4b4c004dull);
  expected_[1].inner_bounds.orows = 78;
  expected_[1].inner_bounds.prad = 79;
  expected_[1].inner_bounds.pupad = 80;
  expected_[1].inner_bounds.pdpad = 81;
  expected_[1].outer_bounds.kernel_dilation = 82;
  expected_[1].inner_bounds.ocols = 83;
  expected_[1].outer_bounds.in_stride = 84;
  expected_[1].outer_bounds.weight_stride = 85;
  expected_[1].outer_bounds.out_stride = 86;
  program_[10] = command(19, 0x4e004f0a014452ull, 0x54005500560053ull);
  expected_[1].weights_dram_addr = 0xb000123456789abcull;
  expected_[1].output_dram_addr = 0xb00023456789abcdull;
  expected_[1].bias_dram_addr = 0xb0003456789abcdeull;
  expected_[1].input_dram_addr = 0xb000456789abcdefull;
  program_[11] = command(20, expected_[1].weights_dram_addr, expected_[1].output_dram_addr);
  program_[12] = command(21, expected_[1].bias_dram_addr, expected_[1].input_dram_addr);

  // Launch flags are saved separately from the bounds; zero pixel count becomes one.
  program_[6] = command(15, 0x6007f, 0x0e);
  expected_[0].launch.no_bias = 1;
  expected_[0].launch.wrot180 = smesh::kDefaultConfig.has_training_convs;
  expected_[0].launch.trans_output_1203 = smesh::kDefaultConfig.has_training_convs;
  expected_[0].launch.trans_weight_1203 = smesh::kDefaultConfig.has_training_convs;
  expected_[0].launch.trans_weight_0132 = smesh::kDefaultConfig.has_training_convs;
  expected_[0].launch.trans_input_3120 = smesh::kDefaultConfig.has_training_convs;
  expected_[0].launch.dw = smesh::kDefaultConfig.has_dw_convs;
  expected_[0].launch.no_pool = !smesh::kDefaultConfig.has_max_pool;
  expected_[0].launch.downsample = 1;
  expected_[0].launch.input_dilated = smesh::kDefaultConfig.has_training_convs;
  expected_[0].launch.activation = 1;
  expected_[0].launch.max_pixels_per_row = 1;
  expected_[0].launch.a_ex_spad_id = 1;
  expected_[0].launch.b_ex_spad_id = 2;

  program_[13] = command(15, 0x90700, 0x11);
  expected_[1].launch.no_pool = 1;
  expected_[1].launch.activation = 2;
  expected_[1].launch.max_pixels_per_row = smesh::kDefaultConfig.has_first_layer_optimizations ? 7 : 1;
  expected_[1].launch.a_ex_spad_id = 2;
  expected_[1].launch.b_ex_spad_id = 1;
  // A third setup cannot overwrite either configured bank.
  program_[14] = command(16, ~std::uint64_t{0}, ~std::uint64_t{0});
  for (std::size_t id = 0; id < expected_.size(); ++id) {
    expected_[id].configured = 1;
    expected_[id].a_addr_start = id * (smesh::kSpRows / 2);
    expected_[id].b_addr_end = (id + 1) * (smesh::kSpRows / 2);
    if (!smesh::kDefaultConfig.has_max_pool) {
      expected_[id].outer_bounds.pool_size = 1;
      expected_[id].outer_bounds.pool_stride = 1;
      expected_[id].outer_bounds.pool_padding = 0;
    }
  }
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(in_val, in_bits, out_rdy);
  UPDATE(updateCheck).reads(cycle_Q_, in_rdy, out_val, busy, loop0, loop1)
      .reads(head_loop_id, tail_loop_id, config_loop_id);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

// Keep downstream blocked: setup acceptance must not depend on output ready.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  in_val = bit(cycle < program_.size());
  in_bits = cycle < program_.size() ? program_[cycle] : smesh::SmeshQueuedCmd{};
  out_rdy = 0;
}

bool sameSlot(const smesh::LoopConvSlot& actual, const smesh::LoopConvSlot& expected) {
  return
      actual.outer_bounds.batch_size == expected.outer_bounds.batch_size &&
      actual.outer_bounds.in_row_dim == expected.outer_bounds.in_row_dim &&
      actual.outer_bounds.in_col_dim == expected.outer_bounds.in_col_dim &&
      actual.outer_bounds.in_channels == expected.outer_bounds.in_channels &&
      actual.outer_bounds.out_channels == expected.outer_bounds.out_channels &&
      actual.outer_bounds.out_col_dim == expected.outer_bounds.out_col_dim &&
      actual.outer_bounds.out_row_dim == expected.outer_bounds.out_row_dim &&
      actual.outer_bounds.out_stride == expected.outer_bounds.out_stride &&
      actual.outer_bounds.in_stride == expected.outer_bounds.in_stride &&
      actual.outer_bounds.weight_stride == expected.outer_bounds.weight_stride &&
      actual.outer_bounds.pool_out_row_dim == expected.outer_bounds.pool_out_row_dim &&
      actual.outer_bounds.pool_out_col_dim == expected.outer_bounds.pool_out_col_dim &&
      actual.outer_bounds.stride == expected.outer_bounds.stride &&
      actual.outer_bounds.padding == expected.outer_bounds.padding &&
      actual.outer_bounds.kernel_dim == expected.outer_bounds.kernel_dim &&
      actual.outer_bounds.kernel_dilation == expected.outer_bounds.kernel_dilation &&
      actual.outer_bounds.pool_size == expected.outer_bounds.pool_size &&
      actual.outer_bounds.pool_stride == expected.outer_bounds.pool_stride &&
      actual.outer_bounds.pool_padding == expected.outer_bounds.pool_padding &&
      actual.inner_bounds.batches == expected.inner_bounds.batches &&
      actual.inner_bounds.porows == expected.inner_bounds.porows &&
      actual.inner_bounds.pocols == expected.inner_bounds.pocols &&
      actual.inner_bounds.pochs == expected.inner_bounds.pochs &&
      actual.inner_bounds.krows == expected.inner_bounds.krows &&
      actual.inner_bounds.kcols == expected.inner_bounds.kcols &&
      actual.inner_bounds.kchs == expected.inner_bounds.kchs &&
      actual.inner_bounds.lpad == expected.inner_bounds.lpad &&
      actual.inner_bounds.rpad == expected.inner_bounds.rpad &&
      actual.inner_bounds.upad == expected.inner_bounds.upad &&
      actual.inner_bounds.dpad == expected.inner_bounds.dpad &&
      actual.inner_bounds.plpad == expected.inner_bounds.plpad &&
      actual.inner_bounds.prad == expected.inner_bounds.prad &&
      actual.inner_bounds.pupad == expected.inner_bounds.pupad &&
      actual.inner_bounds.pdpad == expected.inner_bounds.pdpad &&
      actual.inner_bounds.orows == expected.inner_bounds.orows &&
      actual.inner_bounds.ocols == expected.inner_bounds.ocols &&
      actual.launch.no_bias == expected.launch.no_bias &&
      actual.launch.wrot180 == expected.launch.wrot180 &&
      actual.launch.trans_output_1203 == expected.launch.trans_output_1203 &&
      actual.launch.trans_weight_1203 == expected.launch.trans_weight_1203 &&
      actual.launch.trans_weight_0132 == expected.launch.trans_weight_0132 &&
      actual.launch.trans_input_3120 == expected.launch.trans_input_3120 &&
      actual.launch.dw == expected.launch.dw &&
      actual.launch.no_pool == expected.launch.no_pool &&
      actual.launch.downsample == expected.launch.downsample &&
      actual.launch.input_dilated == expected.launch.input_dilated &&
      actual.launch.activation == expected.launch.activation &&
      actual.launch.max_pixels_per_row == expected.launch.max_pixels_per_row &&
      actual.launch.a_ex_spad_id == expected.launch.a_ex_spad_id &&
      actual.launch.b_ex_spad_id == expected.launch.b_ex_spad_id &&
      actual.weights_dram_addr == expected.weights_dram_addr &&
      actual.output_dram_addr == expected.output_dram_addr &&
      actual.bias_dram_addr == expected.bias_dram_addr &&
      actual.input_dram_addr == expected.input_dram_addr &&
      actual.a_addr_start == expected.a_addr_start &&
      actual.b_addr_end == expected.b_addr_end &&
      actual.configured == expected.configured &&
      actual.running == expected.running &&
      actual.ld_bias_started == expected.ld_bias_started &&
      actual.ld_input_started == expected.ld_input_started &&
      actual.ld_weights_started == expected.ld_weights_started &&
      actual.ex_started == expected.ex_started &&
      actual.st_started == expected.st_started &&
      actual.ld_bias_completed == expected.ld_bias_completed &&
      actual.ld_input_completed == expected.ld_input_completed &&
      actual.ld_weights_completed == expected.ld_weights_completed &&
      actual.ex_completed == expected.ex_completed &&
      actual.st_completed == expected.st_completed;
}

// Check saved state after the clock edge, including preservation across commands.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= 20) return;
  const auto first = *loop0;
  const auto second = *loop1;
  bool match = head_loop_id == 0 && tail_loop_id == 1 &&
               config_loop_id == (cycle < 8 ? 0 : 1) && out_val == bit(cycle >= 11) &&
               first.configured == bit(cycle >= 8) && second.configured == bit(cycle >= 15) &&
               busy == bit(cycle != 0);
  if (cycle < 15) match &= in_rdy == 1;
  if (cycle == 0) {
    smesh::LoopConvSlot empty0{}, empty1{};
    empty0.b_addr_end = smesh::kSpRows / 2;
    empty1.a_addr_start = smesh::kSpRows / 2;
    empty1.b_addr_end = smesh::kSpRows;
    match &= sameSlot(first, empty0) && sameSlot(second, empty1);
  }
  if (cycle == 1) match &= first.outer_bounds.batch_size == 0;
  if (cycle == 2) match &= first.outer_bounds.batch_size == expected_[0].outer_bounds.batch_size;
  if (cycle >= 8) {
    auto expected = expected_[0];
    expected.running = bit(cycle >= 9);
    expected.ld_bias_started = bit(cycle >= 9);
    expected.ld_input_started = bit(cycle >= 9);
    expected.ld_weights_started = bit(cycle >= 9);
    expected.ex_started = bit(cycle >= 10);
    match &= sameSlot(first, expected);
  }
  if (cycle >= 15) match &= sameSlot(second, expected_[1]);
  if (!match) std::printf("LoopConv state mismatch cycle=%u\n", cycle);
  passed_ &= match;
  observed_ |= 1u << cycle;
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

void Driver::reset() {
  cycle_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
  passed_ = true;
  observed_ = 0;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::LoopConv loop("LoopConv");
  Driver driver("Driver");
  loop.in_val << driver.in_val;
  loop.in_bits << driver.in_bits;
  driver.in_rdy << loop.in_rdy;
  loop.out_rdy << driver.out_rdy;
  driver.out_val << loop.out_val;
  driver.busy << loop.busy;
  driver.loop0 << loop.loop0;
  driver.loop1 << loop.loop1;
  driver.head_loop_id << loop.head_loop_id;
  driver.tail_loop_id << loop.tail_loop_id;
  driver.config_loop_id << loop.config_loop_id;
  Clock clk;
  loop.clk << clk;
  driver.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 20; ++cycle) Sim::run();
  bool passed = driver.passed();
  // Repeat after reset to check that both configured banks are cleared.
  Sim::reset();
  for (int cycle = 0; cycle < 20; ++cycle) Sim::run();
  passed &= driver.passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_STATE] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
