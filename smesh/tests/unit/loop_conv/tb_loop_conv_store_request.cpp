// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_store_request.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "LoopConvScheduler.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace {

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
  Input(smesh::LoopConvStReq, req_bits);
  Input(bit, req_fire);
  Input(u8, selected_id);
  void updateDrive();
  void updateCheck();
  void updateCycle();
  void reset();
  static constexpr unsigned kCycles = 24;
  bool passed() const {
    return passed_ && std::all_of(observed_.begin(), observed_.end(), [](bool v) { return v; });
  }
 private:
  static smesh::LoopConvSlot slot(unsigned index);
  std::array<bool, kCycles> observed_{};
  bool passed_ = true;
  Output(u8, cycle_Q_);
  Register(u8, cycle_D_);
};

// Four option combinations: ordinary/pooling and standard/transposed output.
smesh::LoopConvSlot Driver::slot(unsigned index) {
  smesh::LoopConvSlot value{};
  value.configured = value.ex_started = 1;
  // ex_completed and load-start flags stay zero: Store request-valid must not
  // depend on them. Completion will gate command generation in the worker.
  auto& outer = value.outer_bounds;
  outer.batch_size = 3;
  outer.out_channels = 11;
  outer.out_row_dim = 7;
  outer.out_col_dim = 9;
  outer.out_stride = 13;
  outer.pool_out_row_dim = 4;
  outer.pool_out_col_dim = 5;
  outer.pool_size = 2;
  outer.pool_stride = 2;
  outer.pool_padding = 1;
  auto& inner = value.inner_bounds;
  inner.batches = 2;
  inner.orows = 3;
  inner.ocols = 5;
  inner.porows = 2;
  inner.pocols = 3;
  inner.pochs = 7;
  inner.plpad = 1;
  inner.pupad = 2;
  value.output_dram_addr = 0x8123456789abcdefull + index * 0x1000;
  value.launch.no_pool = bit(index % 2 == 0);
  value.launch.trans_output_1203 = bit(index >= 2);
  value.launch.activation = index;
  return value;
}

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(cycle_Q_).writes(loop0, loop1, head_loop_id, req_rdy);
  UPDATE(updateCheck).reads(cycle_Q_, req_val, req_bits, req_fire, selected_id);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

// Missing Execute start, stalled head, accepted head, zero-output tail,
// both Store jobs already started, then an unconfigured tail.
void Driver::updateDrive() {
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  const auto index = cycle / 6;
  const auto phase = cycle % 6;
  const auto head = index % 2;
  auto first = slot(index);
  std::array<smesh::LoopConvSlot, 2> slots{{first, first}};
  // Give the other slot different metadata so incorrect slot selection is visible.
  slots[head ^ 1u].inner_bounds.pochs = 9;
  slots[head ^ 1u].launch.activation = (index + 1) % 4;
  if (phase == 0) {
    slots[head].ex_started = 0;
    // Independently advance either Execute's allocator or bias's allocator
    // while Store is rejected. Neither may change Store's base.
    if (index % 2 == 0) {
      slots[head].ld_bias_started = slots[head].ld_input_started = slots[head].ld_weights_started = 1;
    }
  }
  if (phase >= 3) slots[head].st_started = 1;
  if (phase == 3) slots[head ^ 1u].output_dram_addr = 0;
  if (phase == 4) slots[head ^ 1u].st_started = 1;
  if (phase == 5) slots[head ^ 1u].configured = 0;
  loop0 = slots[0];
  loop1 = slots[1];
  head_loop_id = head;
  req_rdy = bit(phase != 1);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  if (cycle >= kCycles) return;
  const auto index = cycle / 6;
  const auto phase = cycle % 6;
  const auto head = index % 2;
  const bool tail = phase >= 3;
  auto expected = slot(index);
  if (tail) {
    expected.inner_bounds.pochs = 9;
    expected.launch.activation = (index + 1) % 4;
  }
  if (phase == 3) expected.output_dram_addr = 0;
  const auto actual = *req_bits;
  const auto& outer = actual.outer_bounds;
  const auto& inner = actual.inner_bounds;
  const auto base = ((index + unsigned(tail)) % 2) * (smesh::kAccRows / 2);
  const bool valid = phase == 1 || phase == 2 || phase == 3;
  const bool match = req_val == bit(valid) && req_fire == bit(phase == 2 || phase == 3) &&
      actual.loop_id == (tail ? head ^ 1u : head) && selected_id == actual.loop_id &&
      actual.addr_start == base && actual.dram_addr == expected.output_dram_addr &&
      actual.ochs == expected.inner_bounds.pochs && actual.activation == expected.launch.activation &&
      actual.no_pool == expected.launch.no_pool && actual.trans_output_1203 == expected.launch.trans_output_1203 &&
      outer.batch_size == 3 && outer.out_channels == 11 && outer.out_row_dim == 7 && outer.out_col_dim == 9 &&
      outer.out_stride == 13 && outer.pool_out_row_dim == 4 && outer.pool_out_col_dim == 5 &&
      outer.pool_size == 2 && outer.pool_stride == 2 && outer.pool_padding == 1 &&
      inner.batches == 2 && inner.orows == 3 && inner.ocols == 5 &&
      inner.porows == 2 && inner.pocols == 3 && inner.pochs == actual.ochs && inner.plpad == 1 && inner.pupad == 2;
  if (!match) std::printf("Store request mismatch case=%u phase=%u base=%u expected=%u\n",
                         index, phase, actual.addr_start, unsigned(base));
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
  observed_.fill(false);
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
  scheduler.ld_bias_req_rdy.wireToConst(bit(1));
  scheduler.ld_input_req_rdy.wireToConst(bit(0));
  scheduler.ld_weights_req_rdy.wireToConst(bit(0));
  scheduler.ex_req_rdy.wireToConst(bit(1));
  scheduler.st_req_rdy << driver.req_rdy;
  driver.req_val << scheduler.st_req_val;
  driver.req_bits << scheduler.st_req_bits;
  driver.req_fire << scheduler.st_req_fire;
  driver.selected_id << scheduler.st_loop_id;
  Clock clk;
  driver.clk << clk;
  scheduler.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool passed = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < Driver::kCycles; ++cycle) Sim::run();
    passed &= driver.passed();
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_STORE_REQUEST] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
