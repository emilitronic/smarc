// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_execute_request.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "LoopConvScheduler.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

struct Test {
  smesh::LoopConvSlot slot{};
  smesh::LoopConvInputGeometry input{};
  smesh::LoopConvWeightGeometry weight{};
};

std::vector<Test> cases() {
  smesh::LoopConvSlot slot{};
  slot.configured = 1;
  slot.ld_bias_started = slot.ld_input_started = slot.ld_weights_started = 1;
  // Completion flags stay clear: request acceptance requires starts, not finishes.
  slot.outer_bounds.stride = slot.outer_bounds.kernel_dilation = 1;
  slot.inner_bounds.orows = 2;
  slot.inner_bounds.ocols = 3;
  slot.inner_bounds.krows = slot.inner_bounds.kcols = 3;
  slot.inner_bounds.batches = 2;
  slot.inner_bounds.kchs = 7;
  slot.inner_bounds.pochs = 5;
  slot.a_addr_start = 5;
  slot.b_addr_end = smesh::kSpRows;
  slot.output_dram_addr = 0x80007000;
  slot.launch.max_pixels_per_row = 3;
  const auto out_groups = static_cast<std::uint16_t>((5 + smesh::kDim - 1) / smesh::kDim);
  const auto in_groups = static_cast<std::uint16_t>((7 + smesh::kDim - 1) / smesh::kDim);
  std::vector<Test> tests{{slot, {4, 5, 4, 5, 7, 40}, {5, out_groups, in_groups, 63}}};
  slot.inner_bounds.upad = slot.inner_bounds.dpad = 1;
  slot.inner_bounds.lpad = 2;
  slot.inner_bounds.rpad = 1;
  slot.outer_bounds.kernel_dilation = 2;
  tests.push_back({slot, {6, 7, 4, 4, 7, 84}, tests[0].weight});
  slot.outer_bounds.kernel_dilation = 1;
  slot.launch.input_dilated = 1;
  tests.push_back({slot, {3, 3, 1, 1, 7, 18}, tests[0].weight});
  slot = tests[0].slot;
  slot.launch.trans_input_3120 = slot.launch.trans_weight_0132 = 1;
  slot.launch.downsample = slot.launch.wrot180 = 1;
  tests.push_back({slot, {4, 5, 4, 5, 7, 28}, {5, out_groups, in_groups, 45}});
  slot = tests[0].slot;
  // Arithmetic-only case: geometry need not fit the small test memories.
  slot.inner_bounds.orows = slot.inner_bounds.ocols = 400;
  slot.inner_bounds.krows = slot.inner_bounds.kcols = 1;
  tests.push_back({slot, {400, 400, 400, 400, 7, 57856}, {5, out_groups, in_groups, 7}});
  for (unsigned i = 0; i < tests.size(); ++i) {
    tests[i].slot.launch.a_ex_spad_id = i % 4;
    tests[i].slot.launch.b_ex_spad_id = i % 4;
  }
  return tests;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(smesh::LoopConvSlot, loop0);
  Output(smesh::LoopConvSlot, loop1);
  Output(u8, head_loop_id);
  Output(bit, req_rdy);
  Input(bit, req_val);
  Input(smesh::LoopConvExecuteReq, req_bits);
  Input(bit, req_fire);
  Input(u8, selected_id);
  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  unsigned cycles() const { return tests_.size() * 6; }
  bool passed() const {
    return passed_ && std::all_of(observed_.begin(), observed_.end(), [](bool v) { return v; });
  }
 private:
  const std::vector<Test> tests_ = cases();
  std::vector<bool> observed_ = std::vector<bool>(cycles(), false);
  bool passed_ = true;
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(loop0, loop1, head_loop_id, req_rdy);
  UPDATE(updateCheck).reads(cycle_Q_, req_val, req_bits, req_fire, selected_id);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

// Six phases: missing load start, stalled head, accepted head, accepted tail
// without an output, both already started, then an unconfigured tail.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto index = cycle / 6;
  const auto phase = cycle % 6;
  const auto head = index % 2;
  smesh::LoopConvSlot slots[2]{};
  if (index < tests_.size()) {
    slots[0] = slots[1] = tests_[index].slot;
    if (phase == 0) {
      if (index % 3 == 0) slots[head].ld_bias_started = 0;
      if (index % 3 == 1) slots[head].ld_input_started = 0;
      if (index % 3 == 2) slots[head].ld_weights_started = 0;
    }
    if (phase >= 3) slots[head].ex_started = 1;
    if (phase == 3) slots[head ^ 1u].output_dram_addr = 0;
    if (phase == 4) slots[head ^ 1u].ex_started = 1;
    if (phase == 5) slots[head ^ 1u].configured = 0;
  }
  loop0 = slots[0];
  loop1 = slots[1];
  head_loop_id = head;
  req_rdy = bit(phase != 1);
}

// Check C before and after the acceptance edge. A stalled or rejected request
// cannot allocate another half; the tail's zero-output job must reuse that half.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto index = cycle / 6;
  const auto phase = cycle % 6;
  if (index >= tests_.size()) return;
  const auto& test = tests_[index];
  const auto actual = *req_bits;
  const auto& in = actual.input_geometry;
  const auto& weight = actual.weight_geometry;
  const auto head = index % 2;
  const auto selected = phase >= 3 ? head ^ 1u : head;
  const std::uint32_t a_bases[] = {5, 0, smesh::kSpRows / 2, 0};
  const std::uint32_t b_ends[] = {smesh::kSpRows, smesh::kSpRows / 2, smesh::kSpRows, 3 * smesh::kSpRows / 2};
  const auto c = ((index + unsigned(phase >= 3)) % 2) * (smesh::kAccRows / 2);
  const bool valid = phase == 1 || phase == 2 || phase == 3;
  const bool match = req_val == bit(valid) && req_fire == bit(phase == 2 || phase == 3) &&
      actual.loop_id == selected && selected_id == selected && actual.c_addr_start == c &&
      actual.a_addr_start == a_bases[index % 4] && actual.b_addr_end == b_ends[index % 4] &&
      actual.outer_bounds.stride == test.slot.outer_bounds.stride &&
      actual.outer_bounds.kernel_dilation == test.slot.outer_bounds.kernel_dilation &&
      actual.inner_bounds.orows == test.slot.inner_bounds.orows &&
      actual.inner_bounds.kcols == test.slot.inner_bounds.kcols &&
      actual.wrot180 == test.slot.launch.wrot180 && actual.downsample == test.slot.launch.downsample &&
      actual.max_pixels_per_row == 3 && actual.input_dilated == test.slot.launch.input_dilated &&
      actual.trans_weight_0132 == test.slot.launch.trans_weight_0132 &&
      actual.trans_input_3120 == test.slot.launch.trans_input_3120 &&
      in.irows == test.input.irows && in.icols == test.input.icols &&
      in.irows_unpadded == test.input.irows_unpadded && in.icols_unpadded == test.input.icols_unpadded &&
      in.ichs == 7 && in.input_spad_stride == test.input.input_spad_stride &&
      weight.ochs == test.weight.ochs && weight.out_channels_per_bank == test.weight.out_channels_per_bank &&
      weight.in_channels_per_bank == test.weight.in_channels_per_bank &&
      weight.weight_spad_stride == test.weight.weight_spad_stride;
  if (!match) std::printf("Execute request mismatch case=%u phase=%u C=%u expected=%u\n",
                         index, phase, actual.c_addr_start, unsigned(c));
  passed_ &= match;
  observed_[cycle] = true;
}

void Driver::updateCycle() { cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1; }

void Driver::reset() {
  cycle_D_.reset(0);
  loop0.reset(smesh::LoopConvSlot{});
  loop1.reset(smesh::LoopConvSlot{});
  head_loop_id.reset(0);
  req_rdy.reset(0);
  passed_ = true;
  std::fill(observed_.begin(), observed_.end(), false);
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConvScheduler scheduler("Scheduler");
  scheduler.loop0 << driver.loop0;
  scheduler.loop1 << driver.loop1;
  scheduler.head_loop_id << driver.head_loop_id;
  // Bias acceptance can advance its own allocator while Execute is blocked.
  // Execute's C sequence must remain independent of that activity.
  scheduler.ld_bias_req_rdy.wireToConst(bit(1));
  scheduler.ld_input_req_rdy.wireToConst(bit(0));
  scheduler.ld_weights_req_rdy.wireToConst(bit(0));
  scheduler.ex_req_rdy << driver.req_rdy;
  driver.req_val << scheduler.ex_req_val;
  driver.req_bits << scheduler.ex_req_bits;
  driver.req_fire << scheduler.ex_req_fire;
  driver.selected_id << scheduler.ex_loop_id;
  Clock clk;
  driver.clk << clk;
  scheduler.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool passed = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < driver.cycles(); ++cycle) Sim::run();
    passed &= driver.passed();
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_EXECUTE_REQUEST] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
