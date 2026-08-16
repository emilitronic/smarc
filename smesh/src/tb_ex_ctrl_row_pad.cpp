// **********************************************************************
// smesh/src/tb_ex_ctrl_row_pad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 15 2026
// Focused ExCtrlRowPad row-padding validity test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlRowPad.hpp"

#include <cstdio>

class RowPadDriver : public Component {
  DECLARE_COMPONENT(RowPadDriver);

 public:
  RowPadDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(u32, a_fire_counter);
  Output(u32, b_fire_counter);
  Output(u32, d_fire_counter);
  Output(u16, a_rows);
  Output(u16, b_rows);
  Output(u16, d_rows);
  Output(u32, block_size);

  void update();
  void reset();
};

class RowPadMonitor : public Component {
  DECLARE_COMPONENT(RowPadMonitor);

 public:
  RowPadMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, a_row_is_not_all_zeros);
  Input(bit, b_row_is_not_all_zeros);
  Input(bit, d_row_is_not_all_zeros);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

RowPadDriver::RowPadDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(a_fire_counter,
              b_fire_counter,
              d_fire_counter,
              a_rows,
              b_rows,
              d_rows,
              block_size);
}

void RowPadDriver::update() {
  a_fire_counter = 2;
  b_fire_counter = 3;
  d_fire_counter = 1;
  a_rows = 3;
  b_rows = 3;
  d_rows = 2;
  block_size = 4;
}

void RowPadDriver::reset() {
  a_fire_counter.reset(0);
  b_fire_counter.reset(0);
  d_fire_counter.reset(0);
  a_rows.reset(0);
  b_rows.reset(0);
  d_rows.reset(0);
  block_size.reset(0);
}

RowPadMonitor::RowPadMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(a_row_is_not_all_zeros,
                       b_row_is_not_all_zeros,
                       d_row_is_not_all_zeros);
}

void RowPadMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  passed_ =
      a_row_is_not_all_zeros != 0 &&
      b_row_is_not_all_zeros == 0 &&
      d_row_is_not_all_zeros == 0;
  checked_ = true;
  done_ = true;
}

void RowPadMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  RowPadDriver driver("Driver");
  smesh::ExCtrlRowPad row_pad("RowPad");
  RowPadMonitor monitor("Monitor");

  row_pad.a_fire_counter << driver.a_fire_counter;
  row_pad.b_fire_counter << driver.b_fire_counter;
  row_pad.d_fire_counter << driver.d_fire_counter;
  row_pad.a_rows << driver.a_rows;
  row_pad.b_rows << driver.b_rows;
  row_pad.d_rows << driver.d_rows;
  row_pad.block_size << driver.block_size;

  monitor.a_row_is_not_all_zeros << row_pad.a_row_is_not_all_zeros;
  monitor.b_row_is_not_all_zeros << row_pad.b_row_is_not_all_zeros;
  monitor.d_row_is_not_all_zeros << row_pad.d_row_is_not_all_zeros;

  Clock clk;
  driver.clk << clk;
  row_pad.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_ROW_PAD] %s row_padding\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
