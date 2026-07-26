// **********************************************************************
// smesh/src/tb_ex_ctrl.cpp
// **********************************************************************
// Focused ExCtrl command/completion handshake test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrl.hpp"

#include <cstdio>

class ExCtrlDriver : public Component {
  DECLARE_COMPONENT(ExCtrlDriver);

 public:
  ExCtrlDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshIssue, cmd_out);
  FifoInput(smesh::SmeshRsTag, completed_in);

  void update_issue() {
    if (sent_ || cmd_out.full()) {
      return;
    }

    smesh::SmeshIssue issue{};
    issue.rs_tag = expected_rs_tag_;
    cmd_out.push(issue);
    sent_ = true;
  }

  void update_completion() {
    if (completed_in.empty()) {
      return;
    }

    const auto rs_tag = completed_in.pop();
    matched_ = rs_tag == expected_rs_tag_;
    done_ = true;
  }

  void reset() {
    sent_ = false;
    done_ = false;
    matched_ = false;
  }

  bool done() const { return done_; }
  bool matched() const { return matched_; }

 private:
  static constexpr smesh::SmeshRsTag expected_rs_tag_ = 7;
  bool sent_ = false;
  bool done_ = false;
  bool matched_ = false;
};

ExCtrlDriver::ExCtrlDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update_issue).writes(cmd_out);
  UPDATE(update_completion).reads(completed_in);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrl ctrl("ExCtrl");
  ExCtrlDriver driver("Driver");

  ctrl.cmd_in << driver.cmd_out;
  driver.completed_in << ctrl.completed;
  ctrl.cmd_in.setDelay(1);

  Clock clk;
  ctrl.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8 && !driver.done(); ++i) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.matched();
  std::printf("[EX_CTRL] %s handshake\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
