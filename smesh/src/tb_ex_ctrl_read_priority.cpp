// **********************************************************************
// smesh/src/tb_ex_ctrl_read_priority.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
// Focused ExCtrlReadPriority arbitration test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlReadPriority.hpp"

#include <cstdio>

namespace {

constexpr int kCaseCount = 10;

struct PriorityCase {
  const char* name = "";
  smesh::ExCtrlOperand a{};
  smesh::ExCtrlOperand b{};
  smesh::ExCtrlOperand d{};
  std::uint32_t total_rows = 4;
  bit im2col_wire = 0;
  bit im2col_en = 0;
  bit expected_a_valid = 1;
  bit expected_b_valid = 1;
  bit expected_d_valid = 1;
};

smesh::ExCtrlOperand operand(smesh::SmeshLocalAddr addr,
                             std::uint8_t priority,
                             std::uint32_t counter = 0) {
  smesh::ExCtrlOperand result{};
  result.addr = addr;
  result.start_inputting = 1;
  result.counter = counter;
  result.started = 1;
  result.priority = priority;
  return result;
}

PriorityCase priorityCase(int index) {
  PriorityCase test{};
  test.a = operand(smesh::makeSpAddr(0), 0);
  test.b = operand(smesh::makeSpAddr(smesh::kSpBankRows), 1);
  test.d = operand(smesh::makeSpAddr(2 * smesh::kSpBankRows), 2);

  switch (index) {
    case 0:
      test.name = "independent banks";
      break;

    case 1:
      test.name = "shared spad bank";
      test.b.addr = smesh::makeSpAddr(1);
      test.d.addr = smesh::makeSpAddr(2);
      test.expected_b_valid = 0;
      test.expected_d_valid = 0;
      break;

    case 2:
      test.name = "accum contention";
      test.a.addr = smesh::makeAccAddr(0);
      test.b.addr = smesh::makeAccAddr(smesh::kAccBankRows);
      test.d.addr = smesh::makeSpAddr(0);
      test.expected_b_valid = 0;
      break;

    case 3:
      test.name = "accum versus spad";
      test.a.addr = smesh::makeAccAddr(0);
      test.b.addr = smesh::makeSpAddr(0);
      test.d.addr = smesh::makeSpAddr(smesh::kSpBankRows);
      break;

    case 4:
      test.name = "one row ahead";
      test.a.counter = 2;
      test.b.counter = 1;
      test.d.counter = 1;
      test.expected_a_valid = 0;
      break;

    case 5:
      test.name = "wraparound ahead";
      test.a.counter = 0;
      test.b.counter = 3;
      test.d.counter = 3;
      test.expected_a_valid = 0;
      break;

    case 6:
      test.name = "garbage suppression";
      test.b.addr = smesh::makeSpAddr(1);
      test.a.is_garbage = 1;
      break;

    case 7:
      test.name = "inactive suppression";
      test.b.addr = smesh::makeSpAddr(1);
      test.a.start_inputting = 0;
      break;

    case 8:
      test.name = "im2col suppression";
      test.b.addr = smesh::makeSpAddr(1);
      test.a.can_be_im2colled = 1;
      test.im2col_wire = 1;
      test.im2col_en = 1;
      break;

    case 9:
      test.name = "B priority over D";
      test.a.start_inputting = 0;
      test.b.addr = smesh::makeSpAddr(1);
      test.d.addr = smesh::makeSpAddr(2);
      test.expected_d_valid = 0;
      break;

    default:
      test.name = "invalid case";
      test.expected_a_valid = 0;
      test.expected_b_valid = 0;
      test.expected_d_valid = 0;
      break;
  }

  return test;
}

TraceKey(read_priority_view);

} // namespace

class ReadPriorityDriver : public Component {
  DECLARE_COMPONENT(ReadPriorityDriver);

 public:
  ReadPriorityDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(smesh::ExCtrlOperand, a_operand);
  Output(smesh::ExCtrlOperand, b_operand);
  Output(smesh::ExCtrlOperand, d_operand);
  Output(u32, total_rows);
  Output(bit, im2col_wire);
  Output(bit, im2col_en);

  void update();
  void reset();

 private:
  int cycle_ = 0;
};

class ReadPriorityMonitor : public Component {
  DECLARE_COMPONENT(ReadPriorityMonitor);

 public:
  ReadPriorityMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, a_valid);
  Input(bit, b_valid);
  Input(bit, d_valid);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  int cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

ReadPriorityDriver::ReadPriorityDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(a_operand, b_operand, d_operand, total_rows, im2col_wire, im2col_en);
}

void ReadPriorityDriver::update() {
  const auto test = priorityCase(cycle_);
  a_operand = test.a;
  b_operand = test.b;
  d_operand = test.d;
  total_rows = test.total_rows;
  im2col_wire = test.im2col_wire;
  im2col_en = test.im2col_en;
  ++cycle_;
}

void ReadPriorityDriver::reset() {
  cycle_ = 0;
  a_operand.reset(smesh::ExCtrlOperand{});
  b_operand.reset(smesh::ExCtrlOperand{});
  d_operand.reset(smesh::ExCtrlOperand{});
  total_rows.reset(0);
  im2col_wire.reset(0);
  im2col_en.reset(0);
}

ReadPriorityMonitor::ReadPriorityMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(a_valid, b_valid, d_valid);
}

void ReadPriorityMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  const auto test = priorityCase(cycle_);
  const bool case_passed =
      a_valid == test.expected_a_valid &&
      b_valid == test.expected_b_valid &&
      d_valid == test.expected_d_valid;

  s_trace(read_priority_view,
          "case=%02d %-20s got{%u%u%u} expected{%u%u%u}\n",
          cycle_,
          test.name,
          static_cast<unsigned>(a_valid != 0),
          static_cast<unsigned>(b_valid != 0),
          static_cast<unsigned>(d_valid != 0),
          static_cast<unsigned>(test.expected_a_valid != 0),
          static_cast<unsigned>(test.expected_b_valid != 0),
          static_cast<unsigned>(test.expected_d_valid != 0));

  if (!case_passed) {
    std::printf("[EX_CTRL_READ_PRIORITY] case %d (%s) got {%u,%u,%u}, expected {%u,%u,%u}\n",
                cycle_,
                test.name,
                static_cast<unsigned>(a_valid != 0),
                static_cast<unsigned>(b_valid != 0),
                static_cast<unsigned>(d_valid != 0),
                static_cast<unsigned>(test.expected_a_valid != 0),
                static_cast<unsigned>(test.expected_b_valid != 0),
                static_cast<unsigned>(test.expected_d_valid != 0));
    passed_ = false;
  }

  ++cycle_;
  done_ = cycle_ == kCaseCount;
}

void ReadPriorityMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  ReadPriorityDriver driver("Driver");
  smesh::ExCtrlReadPriority priority("ReadPriority");
  ReadPriorityMonitor monitor("Monitor");

  priority.a_operand << driver.a_operand;
  priority.b_operand << driver.b_operand;
  priority.d_operand << driver.d_operand;
  priority.total_rows << driver.total_rows;
  priority.im2col_wire << driver.im2col_wire;
  priority.im2col_en << driver.im2col_en;

  monitor.a_valid << priority.a_valid;
  monitor.b_valid << priority.b_valid;
  monitor.d_valid << priority.d_valid;

  Clock clk;
  driver.clk << clk;
  priority.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < kCaseCount + 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_READ_PRIORITY] %s arbitration\n", ok ? "PASS" : "FAIL");
  descore::flushLog();
  return ok ? 0 : 1;
}
