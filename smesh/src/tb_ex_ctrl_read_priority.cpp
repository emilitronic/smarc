// **********************************************************************
// smesh/src/tb_ex_ctrl_read_priority.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
// Focused ExCtrlReadPriority skeleton test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlReadPriority.hpp"

#include <cstdio>

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
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

ReadPriorityDriver::ReadPriorityDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(a_operand, b_operand, d_operand, total_rows, im2col_wire, im2col_en);
}

void ReadPriorityDriver::update() {
  smesh::ExCtrlOperand a{};
  a.addr = smesh::makeSpAddr(0);
  a.start_inputting = 1;
  a.priority = 0;

  smesh::ExCtrlOperand b{};
  b.addr = smesh::makeSpAddr(1);
  b.start_inputting = 1;
  b.priority = 1;

  smesh::ExCtrlOperand d{};
  d.addr = smesh::makeSpAddr(2);
  d.start_inputting = 1;
  d.priority = 2;

  a_operand = a;
  b_operand = b;
  d_operand = d;
  total_rows = 4;
  im2col_wire = 0;
  im2col_en = 0;
}

void ReadPriorityDriver::reset() {
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
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  passed_ = a_valid == 0 && b_valid == 0 && d_valid == 0;
  checked_ = true;
  done_ = true;
}

void ReadPriorityMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
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
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_READ_PRIORITY] %s skeleton\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
