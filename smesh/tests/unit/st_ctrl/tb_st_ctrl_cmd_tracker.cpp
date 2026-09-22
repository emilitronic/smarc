// **********************************************************************
// smesh/tests/unit/st_ctrl/tb_st_ctrl_cmd_tracker.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
// Focused StoreController command-tracker lifecycle test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "StCtrlCmdTracker.hpp"

#include <cstdio>

namespace {

class TrackerDriver : public Component {
  DECLARE_COMPONENT(TrackerDriver);

 public:
  TrackerDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, alloc_val);
  Output(u32, alloc_response_count);
  Output(smesh::SmeshRsTag, alloc_rs_tag);
  Output(bit, returned_val);
  Output(u16, returned_cmd_id);
  Output(u32, returned_response_count);
  Output(bit, completed_rdy);

  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class TrackerMonitor : public Component {
  DECLARE_COMPONENT(TrackerMonitor);

 public:
  TrackerMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, alloc_rdy);
  Input(u16, alloc_cmd_id);
  Input(bit, returned_rdy);
  Input(bit, completed_val);
  Input(smesh::SmeshRsTag, completed_bits);
  Input(u16, completed_cmd_id);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

TrackerDriver::TrackerDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(alloc_val,
              alloc_response_count,
              alloc_rs_tag,
              returned_val,
              returned_cmd_id,
              returned_response_count,
              completed_rdy);
}

void TrackerDriver::update() {
  alloc_val = 0;
  alloc_response_count = 0;
  alloc_rs_tag = 0;
  returned_val = 0;
  returned_cmd_id = 0;
  returned_response_count = 1;
  completed_rdy = 0;

  switch (cycle_) {
    case 0:
      alloc_val = 1;
      alloc_response_count = 2;
      alloc_rs_tag = 10;
      break;
    case 1:
      alloc_val = 1;
      alloc_response_count = 1;
      alloc_rs_tag = 20;
      break;
    case 2:
      // Both entries are occupied, so this allocation must be backpressured.
      alloc_val = 1;
      alloc_response_count = 1;
      alloc_rs_tag = 30;
      returned_val = 1;
      returned_cmd_id = 0;
      break;
    case 3:
      returned_val = 1;
      returned_cmd_id = 1;
      break;
    case 5:
      completed_rdy = 1;
      break;
    case 6:
      returned_val = 1;
      returned_cmd_id = 0;
      break;
    case 7:
      completed_rdy = 1;
      break;
    case 8:
      alloc_val = 1;
      alloc_response_count = 1;
      alloc_rs_tag = 30;
      break;
    case 9:
      returned_val = 1;
      returned_cmd_id = 0;
      break;
    case 10:
      completed_rdy = 1;
      break;
    default:
      break;
  }

  ++cycle_;
}

void TrackerDriver::reset() {
  cycle_ = 0;
  alloc_val.reset(0);
  alloc_response_count.reset(0);
  alloc_rs_tag.reset(0);
  returned_val.reset(0);
  returned_cmd_id.reset(0);
  returned_response_count.reset(0);
  completed_rdy.reset(0);
}

TrackerMonitor::TrackerMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(alloc_rdy,
             alloc_cmd_id,
             returned_rdy,
             completed_val,
             completed_bits,
             completed_cmd_id);
}

void TrackerMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  bool ok = returned_rdy == 1;
  switch (cycle_) {
    case 0:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 0 && completed_val == 0;
      break;
    case 1:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 1 && completed_val == 0;
      break;
    case 2:
    case 3:
      ok = ok && alloc_rdy == 0 && completed_val == 0;
      break;
    case 4:
    case 5:
      ok = ok && alloc_rdy == 0 && completed_val == 1 &&
           *completed_cmd_id == 1 && *completed_bits == 20;
      break;
    case 6:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 1 && completed_val == 0;
      break;
    case 7:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 1 &&
           completed_val == 1 && *completed_cmd_id == 0 && *completed_bits == 10;
      break;
    case 8:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 0 && completed_val == 0;
      break;
    case 9:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 1 && completed_val == 0;
      break;
    case 10:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 1 &&
           completed_val == 1 && *completed_cmd_id == 0 && *completed_bits == 30;
      break;
    default:
      ok = ok && alloc_rdy == 1 && *alloc_cmd_id == 0 && completed_val == 0;
      break;
  }

  if (!ok) {
    std::printf("[ST_CTRL_CMD_TRACKER] mismatch cycle=%u alloc={rdy=%u id=%u} returned_rdy=%u complete={val=%u id=%u tag=%u}\n",
                cycle_,
                static_cast<unsigned>(alloc_rdy == 1),
                static_cast<unsigned>(*alloc_cmd_id),
                static_cast<unsigned>(returned_rdy == 1),
                static_cast<unsigned>(completed_val == 1),
                static_cast<unsigned>(*completed_cmd_id),
                static_cast<unsigned>(*completed_bits));
  }

  passed_ = passed_ && ok;
  ++cycle_;
  done_ = cycle_ == 12;
}

void TrackerMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  TrackerDriver driver("Driver");
  smesh::StCtrlCmdTracker tracker("StCtrlCmdTracker");
  TrackerMonitor monitor("Monitor");

  tracker.alloc_val << driver.alloc_val;
  tracker.alloc_response_count << driver.alloc_response_count;
  tracker.alloc_rs_tag << driver.alloc_rs_tag;
  tracker.returned_val << driver.returned_val;
  tracker.returned_cmd_id << driver.returned_cmd_id;
  tracker.returned_response_count << driver.returned_response_count;
  tracker.completed_rdy << driver.completed_rdy;

  monitor.alloc_rdy << tracker.alloc_rdy;
  monitor.alloc_cmd_id << tracker.alloc_cmd_id;
  monitor.returned_rdy << tracker.returned_rdy;
  monitor.completed_val << tracker.completed_val;
  monitor.completed_bits << tracker.completed_bits;
  monitor.completed_cmd_id << tracker.completed_cmd_id;

  Clock clk;
  driver.clk << clk;
  tracker.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 14 && !monitor.done(); ++i) {
    Sim::run();
  }

  descore::flushLog();
  const bool ok = monitor.done() && monitor.passed();
  std::printf("[ST_CTRL_CMD_TRACKER] %s lifecycle\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
