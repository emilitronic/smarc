// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_weight_request.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConvScheduler.hpp"
#include "SmeshTypes.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

struct Test {
  smesh::LoopConvSlot slot{};
  smesh::LoopConvWeightGeometry expected{};
  std::uint32_t end = 0;
};

std::vector<Test> cases() {
  smesh::LoopConvSlot slot{};
  slot.configured = 1;
  // Other workers' progress must not affect weight-job selection.
  slot.ld_bias_started = slot.ld_input_started = 1;
  slot.inner_bounds.pochs = smesh::kDim + 1;
  slot.inner_bounds.kchs = 2 * smesh::kDim;
  slot.inner_bounds.krows = 2;
  slot.inner_bounds.kcols = 3;
  slot.outer_bounds.weight_stride = 37;
  slot.outer_bounds.kernel_dim = 3;
  slot.weights_dram_addr = 0x8123456789abcdefull;
  slot.b_addr_end = smesh::kSpRows;
  std::vector<Test> tests;
  tests.push_back({slot, {std::uint16_t(smesh::kDim + 1), 2, 2,
                         std::uint16_t(12 * smesh::kDim)}, smesh::kSpRows});
  slot.launch.trans_weight_1203 = 1;
  slot.launch.b_ex_spad_id = 1;
  tests.push_back({slot, tests[0].expected, smesh::kSpRows / 2});
  slot.launch.trans_weight_1203 = 0;
  slot.launch.trans_weight_0132 = 1;
  slot.launch.b_ex_spad_id = 2;
  tests.push_back({slot, {std::uint16_t(smesh::kDim + 1), 2, 2,
                         std::uint16_t(6 * (smesh::kDim + 1))}, smesh::kSpRows});
  slot.launch.dw = 1;
  slot.launch.b_ex_spad_id = 3;
  tests.push_back({slot, tests.back().expected, 3 * smesh::kSpRows / 2});
  // A zero source address is still offered: the worker, not the scheduler, skips it.
  slot.weights_dram_addr = 0;
  slot.launch.trans_weight_0132 = slot.launch.dw = 0;
  slot.launch.b_ex_spad_id = 0;
  slot.b_addr_end = 5;
  slot.inner_bounds.pochs = slot.inner_bounds.kchs = smesh::kDim;
  tests.push_back({slot, {std::uint16_t(smesh::kDim), 1, 1,
                         std::uint16_t(6 * smesh::kDim)}, 5});
  // Arithmetic-only case: confirms truncation to Original's 16-bit stride field.
  slot.inner_bounds.krows = slot.inner_bounds.kcols = 256;
  tests.push_back({slot, {std::uint16_t(smesh::kDim), 1, 1, 0}, 5});
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
  Input(smesh::LoopConvLdWeightReq, req_bits);
  Input(bit, req_fire);
  Input(u8, selected_id);
  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  unsigned cycles() const { return tests_.size() * 4; }
  bool passed() const {
    return passed_ && std::all_of(observed_.begin(), observed_.end(), [](bool value) { return value; });
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

// Offer each job stalled then ready; afterward suppress started/unconfigured jobs.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto index = cycle / 4;
  const auto phase = cycle % 4;
  const auto head = index % 2;
  const auto selected = index % 3 == 1 ? head ^ 1u : head;
  smesh::LoopConvSlot slots[2]{};
  if (index < tests_.size()) {
    slots[selected] = tests_[index].slot;
    if (selected != head) slots[head].ld_weights_started = 1;
    if (phase == 2) {
      slots[0].ld_weights_started = slots[1].ld_weights_started = 1;
    }
    if (phase == 3) slots[selected].configured = 0;
  }
  loop0 = slots[0];
  loop1 = slots[1];
  head_loop_id = head;
  req_rdy = bit(phase != 0);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto index = cycle / 4;
  const auto phase = cycle % 4;
  if (index >= tests_.size()) return;
  bool match = req_val == bit(phase < 2) && req_fire == bit(phase == 1);
  if (phase != 2) {
    const auto& test = tests_[index];
    const auto actual = *req_bits;
    const auto head = index % 2;
    const auto selected = index % 3 == 1 ? head ^ 1u : head;
    const auto& geometry = actual.geometry;
    match &= selected_id == selected && actual.loop_id == selected &&
        actual.addr_end == test.end && actual.dram_addr == test.slot.weights_dram_addr &&
        actual.trans_weight_1203 == test.slot.launch.trans_weight_1203 &&
        actual.trans_weight_0132 == test.slot.launch.trans_weight_0132 && actual.dw == test.slot.launch.dw &&
        actual.outer_bounds.weight_stride == 37 && actual.outer_bounds.kernel_dim == 3 &&
        actual.inner_bounds.krows == test.slot.inner_bounds.krows &&
        actual.inner_bounds.kcols == test.slot.inner_bounds.kcols &&
        actual.inner_bounds.kchs == test.slot.inner_bounds.kchs &&
        geometry.ochs == test.expected.ochs &&
        geometry.out_channels_per_bank == test.expected.out_channels_per_bank &&
        geometry.in_channels_per_bank == test.expected.in_channels_per_bank &&
        geometry.weight_spad_stride == test.expected.weight_spad_stride;
  }
  if (!match) std::printf("Weight request mismatch case=%u phase=%u\n", index, phase);
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
  scheduler.ld_bias_req_rdy.wireToConst(bit(0));
  scheduler.ld_input_req_rdy.wireToConst(bit(0));
  scheduler.ex_req_rdy.wireToConst(bit(0));
  scheduler.st_req_rdy.wireToConst(bit(0));
  scheduler.ld_weights_req_rdy << driver.req_rdy;
  driver.req_val << scheduler.ld_weights_req_val;
  driver.req_bits << scheduler.ld_weights_req_bits;
  driver.req_fire << scheduler.ld_weights_req_fire;
  driver.selected_id << scheduler.ld_weights_loop_id;
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
  std::printf("[LOOP_CONV_WEIGHT_REQUEST] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
