// **********************************************************************
// smesh/src/tb_ex_ctrl_row_feed_state.cpp
// **********************************************************************
/*
Focused row-feed counter/state test.

cmake --build build --target tb_ex_ctrl_row_feed_state -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl_row_feed_state -trace '*'/ex_ctrl_row_feed_view
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlRowFeedState.hpp"

#include <cstdio>

class RowFeedDriver : public Component {
  DECLARE_COMPONENT(RowFeedDriver);

 public:
  RowFeedDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, firing);
  Output(bit, a_fire);
  Output(bit, b_fire);
  Output(bit, d_fire);
  Output(u32, total_rows);
  Output(u32, a_addr_stride);
  Output(bit, cntl_rdy);

  void update();
  void reset();
};

RowFeedDriver::RowFeedDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(firing, a_fire, b_fire, d_fire,
                        total_rows, a_addr_stride, cntl_rdy);
}

void RowFeedDriver::update() {
  firing = 1;
  a_fire = 1;
  b_fire = 1;
  d_fire = 1;
  total_rows = 4;
  a_addr_stride = 2;
  cntl_rdy = 1;
}

void RowFeedDriver::reset() {
  firing.reset(0);
  a_fire.reset(0);
  b_fire.reset(0);
  d_fire.reset(0);
  total_rows.reset(4);
  a_addr_stride.reset(2);
  cntl_rdy.reset(0);
}

class RowFeedMonitor : public Component {
  DECLARE_COMPONENT(RowFeedMonitor);

 public:
  RowFeedMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(u32, a_fire_counter);
  Input(u32, b_fire_counter);
  Input(u32, d_fire_counter);
  Input(bit, a_fire_started);
  Input(bit, b_fire_started);
  Input(bit, d_fire_started);
  Input(bit, first);
  Input(u32, a_addr_offset);
  Input(bit, about_to_fire_all_rows);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool saw_first_advance_ = false;
  bool saw_final_beat_ = false;
  bool done_ = false;
  bool passed_ = false;
};

RowFeedMonitor::RowFeedMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(a_fire_counter, b_fire_counter, d_fire_counter,
                       a_fire_started, b_fire_started, d_fire_started,
                       first, a_addr_offset)
                .reads(about_to_fire_all_rows);
}

void RowFeedMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  const auto a_count = static_cast<std::uint32_t>(*a_fire_counter);
  const auto b_count = static_cast<std::uint32_t>(*b_fire_counter);
  const auto d_count = static_cast<std::uint32_t>(*d_fire_counter);

  if (!saw_first_advance_ && a_count == 1 && b_count == 1 && d_count == 1) {
    saw_first_advance_ = a_fire_started != 0 && b_fire_started != 0 &&
                         d_fire_started != 0 && *a_addr_offset == 2 && first == 0;
  }

  if (saw_first_advance_ && a_count == 3 && b_count == 3 && d_count == 3) {
    saw_final_beat_ = about_to_fire_all_rows != 0 && *a_addr_offset == 6;
  }

  if (saw_final_beat_ && a_count == 0 && b_count == 0 && d_count == 0) {
    passed_ = a_fire_started == 0 && b_fire_started == 0 &&
              d_fire_started == 0 && *a_addr_offset == 0 && first != 0;
    done_ = true;
  }
}

void RowFeedMonitor::reset() {
  saw_first_advance_ = false;
  saw_final_beat_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  RowFeedDriver driver("Driver");
  smesh::ExCtrlRowFeedState state("RowFeedState");
  RowFeedMonitor monitor("Monitor");

  state.firing << driver.firing;
  state.a_fire << driver.a_fire;
  state.b_fire << driver.b_fire;
  state.d_fire << driver.d_fire;
  state.total_rows << driver.total_rows;
  state.a_addr_stride << driver.a_addr_stride;
  state.cntl_rdy << driver.cntl_rdy;

  monitor.a_fire_counter << state.a_fire_counter;
  monitor.b_fire_counter << state.b_fire_counter;
  monitor.d_fire_counter << state.d_fire_counter;
  monitor.a_fire_started << state.a_fire_started;
  monitor.b_fire_started << state.b_fire_started;
  monitor.d_fire_started << state.d_fire_started;
  monitor.first << state.first;
  monitor.a_addr_offset << state.a_addr_offset;
  monitor.about_to_fire_all_rows << state.about_to_fire_all_rows;

  Clock clk;
  driver.clk << clk;
  state.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 8 && !monitor.done(); ++cycle) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_ROW_FEED_STATE] %s core_progress\n", ok ? "PASS" : "FAIL");
  descore::flushLog();
  return ok ? 0 : 1;
}
