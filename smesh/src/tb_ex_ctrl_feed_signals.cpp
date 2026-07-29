// **********************************************************************
// smesh/src/tb_ex_ctrl_feed_signals.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
// Focused ExCtrlFeedSignals derived-signal test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlFeedSignals.hpp"

#include <cstdio>

class FeedSignalDriver : public Component {
  DECLARE_COMPONENT(FeedSignalDriver);

 public:
  FeedSignalDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, start_inputting_a);
  Output(bit, start_inputting_b);
  Output(bit, start_inputting_d);
  Output(bit, a_valid);
  Output(bit, b_valid);
  Output(bit, d_valid);
  Output(bit, a_ready);
  Output(bit, b_ready);
  Output(bit, d_ready);
  Output(bit, cntl_a_fire);
  Output(bit, cntl_b_fire);
  Output(bit, cntl_d_fire);
  Output(bit, cntl_first);
  Output(bit, mesh_a_fire);
  Output(bit, mesh_b_fire);
  Output(bit, mesh_d_fire);
  Output(bit, mesh_a_rdy);
  Output(bit, mesh_b_rdy);
  Output(bit, mesh_d_rdy);
  Output(bit, mesh_req_rdy);

  void update();
  void reset();
};

class FeedSignalMonitor : public Component {
  DECLARE_COMPONENT(FeedSignalMonitor);

 public:
  FeedSignalMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, firing);
  Input(bit, a_fire);
  Input(bit, b_fire);
  Input(bit, d_fire);
  Input(bit, mesh_cntl_deq_rdy);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

FeedSignalDriver::FeedSignalDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(start_inputting_a,
              start_inputting_b,
              start_inputting_d,
              a_valid,
              b_valid,
              d_valid,
              a_ready,
              b_ready)
      .writes(d_ready,
              cntl_a_fire,
              cntl_b_fire,
              cntl_d_fire,
              cntl_first,
              mesh_a_fire,
              mesh_b_fire,
              mesh_d_fire)
      .writes(mesh_a_rdy, mesh_b_rdy, mesh_d_rdy, mesh_req_rdy);
}

void FeedSignalDriver::update() {
  start_inputting_a = 0;
  start_inputting_b = 1;
  start_inputting_d = 0;
  a_valid = 1;
  b_valid = 1;
  d_valid = 1;
  a_ready = 1;
  b_ready = 0;
  d_ready = 1;
  cntl_a_fire = 1;
  cntl_b_fire = 1;
  cntl_d_fire = 1;
  cntl_first = 1;
  mesh_a_fire = 1;
  mesh_b_fire = 0;
  mesh_d_fire = 0;
  mesh_a_rdy = 1;
  mesh_b_rdy = 0;
  mesh_d_rdy = 0;
  mesh_req_rdy = 1;
}

void FeedSignalDriver::reset() {
  start_inputting_a.reset(0);
  start_inputting_b.reset(0);
  start_inputting_d.reset(0);
  a_valid.reset(0);
  b_valid.reset(0);
  d_valid.reset(0);
  a_ready.reset(0);
  b_ready.reset(0);
  d_ready.reset(0);
  cntl_a_fire.reset(0);
  cntl_b_fire.reset(0);
  cntl_d_fire.reset(0);
  cntl_first.reset(0);
  mesh_a_fire.reset(0);
  mesh_b_fire.reset(0);
  mesh_d_fire.reset(0);
  mesh_a_rdy.reset(0);
  mesh_b_rdy.reset(0);
  mesh_d_rdy.reset(0);
  mesh_req_rdy.reset(0);
}

FeedSignalMonitor::FeedSignalMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(firing, a_fire, b_fire, d_fire, mesh_cntl_deq_rdy);
}

void FeedSignalMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  passed_ =
      firing != 0 &&
      a_fire != 0 &&
      b_fire == 0 &&
      d_fire != 0 &&
      mesh_cntl_deq_rdy != 0;
  checked_ = true;
  done_ = true;
}

void FeedSignalMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  FeedSignalDriver driver("Driver");
  smesh::ExCtrlFeedSignals signals("FeedSignals");
  FeedSignalMonitor monitor("Monitor");

  signals.start_inputting_a << driver.start_inputting_a;
  signals.start_inputting_b << driver.start_inputting_b;
  signals.start_inputting_d << driver.start_inputting_d;
  signals.a_valid << driver.a_valid;
  signals.b_valid << driver.b_valid;
  signals.d_valid << driver.d_valid;
  signals.a_ready << driver.a_ready;
  signals.b_ready << driver.b_ready;
  signals.d_ready << driver.d_ready;
  signals.cntl_a_fire << driver.cntl_a_fire;
  signals.cntl_b_fire << driver.cntl_b_fire;
  signals.cntl_d_fire << driver.cntl_d_fire;
  signals.cntl_first << driver.cntl_first;
  signals.mesh_a_fire << driver.mesh_a_fire;
  signals.mesh_b_fire << driver.mesh_b_fire;
  signals.mesh_d_fire << driver.mesh_d_fire;
  signals.mesh_a_rdy << driver.mesh_a_rdy;
  signals.mesh_b_rdy << driver.mesh_b_rdy;
  signals.mesh_d_rdy << driver.mesh_d_rdy;
  signals.mesh_req_rdy << driver.mesh_req_rdy;

  monitor.firing << signals.firing;
  monitor.a_fire << signals.a_fire;
  monitor.b_fire << signals.b_fire;
  monitor.d_fire << signals.d_fire;
  monitor.mesh_cntl_deq_rdy << signals.mesh_cntl_deq_rdy;

  Clock clk;
  driver.clk << clk;
  signals.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_FEED_SIGNALS] %s derived_signals\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
