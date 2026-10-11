// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_input_request.cpp
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
  smesh::LoopConvInputGeometry expected{};
};

std::vector<Test> cases() {
  smesh::LoopConvSlot normal{};
  normal.configured = 1;
  normal.ld_bias_started = 1; // Input selection must be independent of bias progress.
  normal.outer_bounds.stride = 1;
  normal.outer_bounds.kernel_dilation = 1;
  normal.inner_bounds.orows = 2;
  normal.inner_bounds.ocols = 3;
  normal.inner_bounds.krows = 3;
  normal.inner_bounds.kcols = 3;
  normal.inner_bounds.batches = 2;
  normal.inner_bounds.kchs = 7;
  normal.input_dram_addr = 0x8123456789abcdefull;
  normal.a_addr_start = 5;
  normal.launch.max_pixels_per_row = 3;
  std::vector<Test> tests;
  tests.push_back({normal, {4, 5, 4, 5, 7, 40}});
  auto padded = normal;
  padded.inner_bounds.upad = 1;
  padded.inner_bounds.dpad = 1;
  padded.inner_bounds.lpad = 2;
  padded.inner_bounds.rpad = 1;
  tests.push_back({padded, {4, 5, 2, 2, 7, 40}});
  auto kernel_dilated = padded;
  kernel_dilated.outer_bounds.kernel_dilation = 2;
  tests.push_back({kernel_dilated, {6, 7, 4, 4, 7, 84}});
  auto input_dilated = padded;
  input_dilated.launch.input_dilated = 1;
  tests.push_back({input_dilated, {3, 3, 1, 1, 7, 18}});
  kernel_dilated.launch.input_dilated = 1;
  tests.push_back({kernel_dilated, {4, 4, 2, 2, 7, 32}});
  auto downsampled = normal;
  downsampled.launch.downsample = 1;
  tests.push_back({downsampled, {4, 5, 4, 5, 7, 8}});
  auto transposed = normal;
  transposed.launch.trans_input_3120 = 1;
  tests.push_back({transposed, {4, 5, 4, 5, 7, 140}});
  transposed.launch.downsample = 1;
  tests.push_back({transposed, {4, 5, 4, 5, 7, 28}});
  auto large = normal;
  large.inner_bounds.orows = large.inner_bounds.ocols = 400;
  large.inner_bounds.krows = large.inner_bounds.kcols = 1;
  tests.push_back({large, {400, 400, 400, 400, 7, 57856}});
  large.inner_bounds.orows = large.inner_bounds.ocols = 65535;
  large.outer_bounds.stride = 2;
  tests.push_back({large, {65534, 65534, 65534, 65534, 7, 8}});
  // Arithmetic-only edge cases: these are not asserted to be legal convolutions.
  auto underflow = normal;
  underflow.inner_bounds.orows = underflow.inner_bounds.ocols = 1;
  underflow.inner_bounds.krows = underflow.inner_bounds.kcols = 1;
  underflow.inner_bounds.upad = 2;
  tests.push_back({underflow, {1, 1, 65535, 1, 7, 2}});
  underflow.launch.input_dilated = 1;
  tests.push_back({underflow, {1, 1, 0, 1, 7, 2}});
  for (std::size_t i = 0; i < tests.size(); ++i)
    tests[i].slot.launch.a_ex_spad_id = i % 4;
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
  Input(smesh::LoopConvLdInputReq, req_bits);
  Input(bit, req_fire);
  Input(u8, selected_id);
  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  std::size_t cycles() const { return tests_.size() * 2; }
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

// Each packet is offered stalled, then accepted; exercise both head IDs and tails.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto index = cycle / 2;
  smesh::LoopConvSlot slots[2]{};
  const auto head = index % 2;
  const bool tail = index % 3 == 1;
  if (index < tests_.size()) {
    slots[tail ? head ^ 1u : head] = tests_[index].slot;
    if (tail) slots[head].ld_input_started = 1;
  }
  loop0 = slots[0];
  loop1 = slots[1];
  head_loop_id = head;
  req_rdy = bit(cycle % 2 == 1);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto index = cycle / 2;
  if (index >= tests_.size()) return;
  const auto& slot = tests_[index].slot;
  const auto& expected = tests_[index].expected;
  const auto actual = *req_bits;
  const auto& geometry = actual.geometry;
  const auto head = index % 2;
  const auto selected = index % 3 == 1 ? head ^ 1u : head;
  const auto spad_id = slot.launch.a_ex_spad_id;
  // At the tested power-of-two capacity, encoded ID 3 truncates back to zero.
  const std::uint32_t bases[] = {5, 0, smesh::kSpRows / 2, 0};
  const auto base = bases[spad_id];
  const bool match = req_val == 1 && req_fire == bit(cycle % 2 == 1) &&
      selected_id == selected && actual.loop_id == selected &&
      actual.addr_start == base && actual.dram_addr == slot.input_dram_addr &&
      actual.downsample == slot.launch.downsample && actual.input_dilated == slot.launch.input_dilated &&
      actual.trans_input_3120 == slot.launch.trans_input_3120 &&
      actual.max_pixels_per_row == slot.launch.max_pixels_per_row &&
      actual.outer_bounds.stride == slot.outer_bounds.stride &&
      actual.inner_bounds.kchs == slot.inner_bounds.kchs &&
      geometry.irows == expected.irows && geometry.icols == expected.icols &&
      geometry.irows_unpadded == expected.irows_unpadded &&
      geometry.icols_unpadded == expected.icols_unpadded &&
      geometry.ichs == expected.ichs && geometry.input_spad_stride == expected.input_spad_stride;
  if (!match)
    std::printf("Input request case=%u phase=%u dims=%u,%u unpadded=%u,%u stride=%u\n",
                index, cycle % 2, geometry.irows, geometry.icols,
                geometry.irows_unpadded, geometry.icols_unpadded, geometry.input_spad_stride);
  passed_ &= match;
  observed_[cycle] = true;
}

void Driver::updateCycle() {
  cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1;
}

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
  scheduler.ld_input_req_rdy << driver.req_rdy;
  driver.req_val << scheduler.ld_input_req_val;
  driver.req_bits << scheduler.ld_input_req_bits;
  driver.req_fire << scheduler.ld_input_req_fire;
  driver.selected_id << scheduler.ld_input_loop_id;
  Clock clk;
  driver.clk << clk;
  scheduler.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (std::size_t cycle = 0; cycle < driver.cycles(); ++cycle) Sim::run();
  const bool passed = driver.passed();
  descore::flushLog();
  std::printf("[LOOP_CONV_INPUT_REQUEST] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
