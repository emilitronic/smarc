// **********************************************************************
// smesh/tests/unit/ex_ctrl/tb_transpose_preload_unroller.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 24 2026
/*
Standalone command expansion, queue timing, tag, and backpressure checks.
  cmake --build build --target tb_transpose_preload_unroller -j
  ./build/smesh/tb_transpose_preload_unroller
*/
#include "TransposePreloadUnroller.hpp"
#include "SmeshCommand.hpp"

#include <descore/Parameter.hpp>
#include <cstdio>
#include <vector>

namespace {
using namespace smesh;

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(SmeshIssue, in_bits);
  Output(bit, out_rdy);

  bool valid = false;
  bool ready = true;
  SmeshIssue data{};
  void update() {
    in_val = bit(valid);
    in_bits = data;
    out_rdy = bit(ready);
  }
  void reset() override {
    valid = false;
    ready = true;
    data = {};
    in_val.reset(0);
    in_bits.reset(SmeshIssue{});
    out_rdy.reset(1);
  }
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(in_val, in_bits, out_rdy);
}

bool equal(const SmeshIssue& a, const SmeshIssue& b) {
  return a.cmd.funct == b.cmd.funct && a.cmd.rs1 == b.cmd.rs1 &&
         a.cmd.rs2 == b.cmd.rs2 && a.rs_tag == b.rs_tag &&
         a.rs_tag_valid == b.rs_tag_valid;
}

SmeshIssue command(SmeshFunct funct, unsigned tag) {
  SmeshIssue value{};
  value.cmd.funct = static_cast<std::uint32_t>(funct);
  // Different shapes/addresses on each operand detect accidental high-bit changes.
  value.cmd.rs1 = packLocal(0x12u + tag, MatrixShape{3, 4});
  value.cmd.rs2 = packLocal(0x24u + tag, MatrixShape{2, 3});
  value.rs_tag = tag;
  return value;
}

SmeshIssue config(bool transpose, unsigned dataflow = kExDataflowWS, bool strides_only = false) {
  auto value = command(SmeshFunct::Config, 1);
  value.cmd.rs1 = packConfigExRs1(2, false, transpose, dataflow, strides_only);
  value.cmd.rs2 = packConfigExRs2(3);
  return value;
}

// Expected packets are stated independently of the component's FSM/queue code.
std::vector<SmeshIssue> expanded(const SmeshIssue& preload, const SmeshIssue& compute) {
  auto first = preload;
  first.cmd.rs2 = packLocal(0xffffffffu, unpackLocal(preload.cmd.rs2).shape);
  first.rs_tag_valid = 0;
  auto helper = compute;
  helper.cmd.funct = static_cast<std::uint32_t>(SmeshFunct::ComputeStay);
  helper.cmd.rs1 = packLocal(0xffffffffu, unpackLocal(compute.cmd.rs1).shape);
  helper.cmd.rs2 = packLocal(0xffffffffu, unpackLocal(compute.cmd.rs2).shape);
  helper.rs_tag_valid = 0;
  auto second = preload;
  second.cmd.rs1 = packLocal(0xffffffffu, unpackLocal(preload.cmd.rs1).shape);
  return {first, helper, second, compute};
}
} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Clock clk;
  Driver driver("Driver");
  TransposePreloadUnroller dut("Unroller");
  driver.clk << clk;
  dut.clk << clk;
  dut.in_val << driver.in_val;
  dut.in_bits << driver.in_bits;
  dut.out_rdy << driver.out_rdy;
  clk.generateClock();
  Sim::init();
  Sim::reset();

  bool ok = true;
  unsigned checks = 0;
  auto check = [&](bool condition, const char* message) {
    ++checks;
    if (!condition) {
      std::printf("  FAIL check=%u: %s\n", checks, message);
      ok = false;
    }
  };
  auto step = [&](const SmeshIssue* input, bool ready, bool input_ready,
                  const SmeshIssue* output) {
    driver.valid = input != nullptr;
    driver.data = input ? *input : SmeshIssue{};
    driver.ready = ready;
    Sim::run();
    check((dut.in_rdy == 1) == input_ready, "queue input-ready timing");
    check((dut.out_val == 1) == (output != nullptr), "output-valid timing");
    if (output && dut.out_val == 1) check(equal(*dut.out_bits, *output), "output payload/tag");
  };

  const auto on = config(true);
  const auto pre = command(SmeshFunct::Preload, 7);
  const auto comp = command(SmeshFunct::ComputeFlip, 8);
  const auto four = expanded(pre, comp);

  // With reset's transpose setting off, even a solitary PRELOAD passes through.
  step(&pre, true, true, nullptr);
  step(&comp, false, true, &pre);
  step(nullptr, false, false, &pre);
  step(nullptr, true, false, &pre);
  step(nullptr, true, true, &comp);
  step(nullptr, true, true, nullptr);

  // CONFIG takes effect when emitted, not merely when accepted at the input.
  Sim::reset();
  step(&on, true, true, nullptr);
  step(&pre, false, true, &on);
  step(nullptr, true, false, &on);
  step(nullptr, true, true, nullptr); // PRELOAD waits for the second queue entry.
  step(&comp, true, true, nullptr);  // No combinational input-to-output bypass.
  for (std::size_t i = 0; i < four.size(); ++i) {
    // Both original commands remain queued until the second PRELOAD leaves.
    const bool input_ready = i == 3;
    step(nullptr, false, input_ready, &four[i]);
    step(nullptr, false, input_ready, &four[i]);
    step(nullptr, true, input_ready, &four[i]);
  }
  step(nullptr, true, true, nullptr);

  // A stream checks consecutive pairs, non-FLIP followers, config filtering,
  // simultaneous enqueue/dequeue, input gaps, and long downstream stalls.
  std::vector<SmeshIssue> program;
  std::vector<SmeshIssue> expected;
  auto pass = [&](const SmeshIssue& packet) {
    program.push_back(packet);
    expected.push_back(packet);
  };
  auto pair = [&](bool expand, bool tagged = true) {
    auto p = pre;
    auto c = comp;
    p.rs_tag_valid = bit(tagged);
    c.rs_tag_valid = bit(tagged);
    program.push_back(p);
    program.push_back(c);
    if (expand) {
      const auto packets = expanded(p, c);
      expected.insert(expected.end(), packets.begin(), packets.end());
    } else {
      expected.push_back(p);
      expected.push_back(c);
    }
  };
  pass(on);
  pair(true);
  pair(true, false);
  pass(config(false, kExDataflowOS, true)); // stride-only must retain expansion.
  auto other_config = on;
  other_config.cmd.rs1 = packConfig(ConfigKind::Im2Col);
  pass(other_config);
  pair(true);
  pass(pre);
  pass(command(SmeshFunct::ComputeStay, 9)); // no expansion for STAY.
  pass(config(true, kExDataflowOS));
  pair(false); // transpose flag alone is insufficient in OS.
  pass(config(false));
  pair(false);
  pass(on);
  pair(true);

  Sim::reset();
  std::size_t sent = 0;
  std::size_t received = 0;
  unsigned enqueue_and_dequeue = 0;
  bool input_pending = false;
  bool held = false;
  SmeshIssue held_packet{};
  for (unsigned cycle = 0; cycle < 600 && received < expected.size(); ++cycle) {
    if (sent < program.size() && cycle % 7 != 0) input_pending = true;
    driver.valid = input_pending;
    driver.data = sent < program.size() ? program[sent] : SmeshIssue{};
    driver.ready = cycle % 13 >= 4;
    Sim::run();
    if (held) check(dut.out_val == 1 && equal(*dut.out_bits, held_packet), "stable stalled output");
    held = dut.out_val == 1 && !driver.ready;
    if (held) held_packet = *dut.out_bits;
    const bool push = driver.valid && dut.in_rdy == 1;
    const bool pop = driver.ready && dut.out_val == 1;
    if (push) {
      ++sent;
      input_pending = false;
    }
    if (pop) {
      check(received < expected.size() && equal(*dut.out_bits, expected[received]), "stream order/payload/tag");
      ++received;
    }
    if (push && pop) ++enqueue_and_dequeue;
  }
  check(sent == program.size() && received == expected.size(), "complete expanded stream");
  check(enqueue_and_dequeue > 0, "exercise simultaneous enqueue/dequeue");
  for (unsigned cycle = 0; cycle < 4; ++cycle) step(nullptr, true, true, nullptr);

  // Reset in each expansion phase discards both retained original commands.
  for (unsigned phase = 0; phase < 3; ++phase) {
    Sim::reset();
    step(&on, true, true, nullptr);
    step(&pre, true, true, &on);
    step(&comp, true, true, nullptr);
    for (unsigned i = 0; i < phase; ++i) step(nullptr, true, false, &four[i]);
    step(nullptr, false, false, &four[phase]);
    Sim::reset();
    step(nullptr, true, true, nullptr);
    step(&pre, true, true, nullptr);
    step(nullptr, true, true, &pre); // expansion disabled again.
  }

  std::printf("[TRANSPOSE_PRELOAD_UNROLLER] %s checks=%u stream_in=%zu stream_out=%zu\n",
              ok ? "PASS" : "FAIL", checks, sent, received);
  descore::flushLog();
  return ok ? 0 : 1;
}
