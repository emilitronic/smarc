// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl_cmd_queue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026
// Focused load command queue test covering fill, full-queue stall, simultaneous pop/push, and drain.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrlQueues.hpp"

#include <cstdio>

namespace {

class QueueDriver : public Component {
  DECLARE_COMPONENT(QueueDriver);

 public:
  QueueDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, cmd_val);
  Output(smesh::SmeshIssue, cmd_bits);
  Output(bit, head_rdy);

  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class QueueMonitor : public Component {
  DECLARE_COMPONENT(QueueMonitor);

 public:
  QueueMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, cmd_rdy);
  Input(bit, head_val);
  Input(smesh::SmeshIssue, head_bits);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

QueueDriver::QueueDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(cmd_val, cmd_bits, head_rdy);
}

void QueueDriver::update() {
  cmd_val = bit(cycle_ < 10);
  head_rdy = bit(cycle_ == 9 || (cycle_ >= 11 && cycle_ <= 18));

  smesh::SmeshIssue issue{};
  issue.rs_tag_valid = 1;
  issue.rs_tag = cycle_ < 8 ? cycle_ + 1 : 9;
  cmd_bits = issue;
  ++cycle_;
}

void QueueDriver::reset() {
  cycle_ = 0;
  cmd_val.reset(0);
  cmd_bits.reset(smesh::SmeshIssue{});
  head_rdy.reset(0);
}

QueueMonitor::QueueMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_rdy, head_val, head_bits);
}

void QueueMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  const unsigned expected_tag = cycle_ == 0 || cycle_ >= 19 ? 0 :
                                cycle_ <= 9 ? 1 :
                                cycle_ == 10 ? 2 :
                                cycle_ <= 17 ? cycle_ - 9 : 9;
  const bool expected_ready = cycle_ < 8 || cycle_ == 9 || cycle_ >= 11;
  const bool valid = head_val == 1;
  const unsigned tag = valid ? static_cast<unsigned>(head_bits->rs_tag) : 0;

  std::printf("[c%02u] in_rdy=%u head{v=%u tag=%u}\n",
              cycle_, static_cast<unsigned>(cmd_rdy == 1),
              static_cast<unsigned>(valid), tag);

  passed_ &= (cmd_rdy == 1) == expected_ready;
  passed_ &= valid == (expected_tag != 0);
  passed_ &= tag == expected_tag;
  done_ = cycle_ == 19;
  ++cycle_;
}

void QueueMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LdCtrlCmdQueue queue("LoadQueue");
  QueueDriver driver("Driver");
  QueueMonitor monitor("Monitor");

  queue.cmd_val << driver.cmd_val;
  queue.cmd_bits << driver.cmd_bits;
  queue.head_rdy << driver.head_rdy;
  monitor.cmd_rdy << queue.cmd_rdy;
  monitor.head_val << queue.head_val;
  monitor.head_bits << queue.head_bits;

  Clock clk;
  queue.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 22 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL_CMD_QUEUE] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
