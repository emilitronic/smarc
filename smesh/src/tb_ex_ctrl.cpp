// **********************************************************************
// smesh/src/tb_ex_ctrl.cpp
// **********************************************************************
/*
Focused ExCtrl command/completion handshake test.

cmake --build build --target tb_ex_ctrl -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrl.hpp"
#include "SmeshCommand.hpp"

#include <cstdio>

namespace {

const char* functName(std::uint32_t funct) {
  switch (static_cast<smesh::SmeshFunct>(funct)) {
    case smesh::SmeshFunct::Config:      return "CFG";
    case smesh::SmeshFunct::Mvin2:       return "M2";
    case smesh::SmeshFunct::Mvin:        return "MVI";
    case smesh::SmeshFunct::Mvout:       return "MVO";
    case smesh::SmeshFunct::ComputeFlip: return "CMPF";
    case smesh::SmeshFunct::ComputeStay: return "CMPS";
    case smesh::SmeshFunct::Preload:     return "PRE";
    case smesh::SmeshFunct::Flush:       return "FLU";
    case smesh::SmeshFunct::Mvin3:       return "M3";
    case smesh::SmeshFunct::StoreSpad:   return "SSP";
  }
  return "---";
}

const char* commandName(bool valid, std::uint32_t funct) {
  return valid ? functName(funct) : "---";
}

} // namespace

class ExCtrlDriver : public Component {
  DECLARE_COMPONENT(ExCtrlDriver);

 public:
  ExCtrlDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshIssue, cmd_out);
  InputArray(bit, head_val, smesh::kExCtrlCmdWindow);
  InputArray(smesh::SmeshIssue, head_bits, smesh::kExCtrlCmdWindow);

  void update_issue() {
    if (Sim::state == Sim::SimResetting || sent_ || cmd_out.full()) {
      return;
    }

    smesh::SmeshIssue issue{};
    issue.rs_tag = expected_rs_tag_;
    issue.cmd.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::Config);
    issue.cmd.rs1 = smesh::packConfigExecuteRs1(1);
    issue.cmd.rs2 = smesh::packConfigExecuteRs2(1);
    cmd_out.push(issue);
    sent_ = true;
  }

  void update_completion() {
    if (Sim::state == Sim::SimResetting) {
      return;
    }

    std::printf("[cycle %d] h0{v=%u t=%03u c=%4s} h1{v=%u t=%03u c=%4s} h2{v=%u t=%03u c=%4s}\n",
                cycle_,
                static_cast<unsigned>(head_val[0]), static_cast<unsigned>(head_bits[0]->rs_tag), commandName(head_val[0] != 0, head_bits[0]->cmd.funct),
                static_cast<unsigned>(head_val[1]), static_cast<unsigned>(head_bits[1]->rs_tag), commandName(head_val[1] != 0, head_bits[1]->cmd.funct),
                static_cast<unsigned>(head_val[2]), static_cast<unsigned>(head_bits[2]->rs_tag), commandName(head_val[2] != 0, head_bits[2]->cmd.funct));
    if (head_val[0] != 0 && head_bits[0]->rs_tag == expected_rs_tag_ &&
        head_bits[0]->cmd.funct == static_cast<std::uint32_t>(smesh::SmeshFunct::Config)) {
      matched_ = true;
      done_ = true;
    }
    ++cycle_;
  }

  void reset() {
    sent_ = false;
    done_ = false;
    matched_ = false;
    cycle_ = 0;
  }

  bool done() const { return done_; }
  bool matched() const { return matched_; }

 private:
  static constexpr smesh::SmeshRsTag expected_rs_tag_ = 7;
  int cycle_ = 0;
  bool sent_ = false;
  bool done_ = false;
  bool matched_ = false;
};

ExCtrlDriver::ExCtrlDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update_issue).writes(cmd_out);
  UPDATE(update_completion).reads(head_val, head_bits);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrl ctrl("ExCtrl");
  ExCtrlDriver driver("Driver");

  ctrl.cmd_in << driver.cmd_out;
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    driver.head_val[i] << ctrl.cmd_queue_head_val[i];
    driver.head_bits[i] << ctrl.cmd_queue_head_bits[i];
  }
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
  std::printf("[EX_CTRL] %s config_reached_cmd_queue\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
