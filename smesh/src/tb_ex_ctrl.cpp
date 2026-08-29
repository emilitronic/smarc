// **********************************************************************
// smesh/src/tb_ex_ctrl.cpp
// **********************************************************************
/*
Focused ExCtrl command/completion handshake test.

cmake --build build --target tb_ex_ctrl -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl

- Queue/testbench view
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_view
- FSM condition inputs
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_state_view
- Completion pending state
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_completion_view
- You can combine them:
./build/smesh/tb_ex_ctrl -trace '*'/ex_ctrl_view';''*'/ex_ctrl_state_view
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrl.hpp"
#include "SmeshCommand.hpp"

#include <cstdio>

TraceKey(ex_ctrl_view); // declare a named TraceKey (and enable it explicitly below)

namespace {

const char* functName(std::uint32_t funct) {
  switch (static_cast<smesh::SmeshFunct>(funct)) {
    case smesh::SmeshFunct::Config:      return "CFG";   // CONFIG
    case smesh::SmeshFunct::Mvin2:       return "M2";    // MVIN2
    case smesh::SmeshFunct::Mvin:        return "MVI";   // MVIN
    case smesh::SmeshFunct::Mvout:       return "MVO";   // MVOUT
    case smesh::SmeshFunct::ComputeFlip: return "CMPF";  // COMPUTE_FLIP
    case smesh::SmeshFunct::ComputeStay: return "CMPS";  // COMPUTE_STAY
    case smesh::SmeshFunct::Preload:     return "PRE";   // PRELOAD
    case smesh::SmeshFunct::Flush:       return "FLU";   // FLUSH
    case smesh::SmeshFunct::Mvin3:       return "M3";    // MVIN3
    case smesh::SmeshFunct::StoreSpad:   return "SSP";   // STORE_SPAD
  }
  return "---";
}

const char* commandName(bool valid, std::uint32_t funct) {
  return valid ? functName(funct) : "---";
}

const char* stateName(std::uint8_t state) {
  switch (static_cast<smesh::ExCtrlFsmState>(state)) {
    case smesh::ExCtrlFsmState::WaitingForCmd: return "WAIT";  // WAITING_FOR_CMD
    case smesh::ExCtrlFsmState::Compute:       return "COMP";  // COMPUTE
    case smesh::ExCtrlFsmState::Flush:         return "FLUS";  // FLUSH
    case smesh::ExCtrlFsmState::Flushing:      return "FLNG";  // FLUSHING
  }
  return "????";
}

} // namespace

class ExCtrlDriver : public Component {
  DECLARE_COMPONENT(ExCtrlDriver);

 public:
  ExCtrlDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshIssue, cmd_out);
  // Test-only taps from ExCtrl; completion is intentionally not checked here.
  Input(u8, control_state);
  Input(bit, config_val);
  Input(bit, config_rs_tag_valid);
  Input(smesh::SmeshRsTag, config_rs_tag);
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
    // emit TraceKey with s_trace
    s_trace(ex_ctrl_view,
          "cycle=%d state=%4s cfg{v=%u tv=%u t=%03u} h0{v=%u t=%03u c=%4s} h1{v=%u t=%03u c=%4s} h2{v=%u t=%03u c=%4s}\n",
                cycle_,
                stateName(static_cast<std::uint8_t>(*control_state)),
                static_cast<unsigned>(config_val),
                static_cast<unsigned>(config_rs_tag_valid),
                static_cast<unsigned>(*config_rs_tag),
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
  UPDATE(update_completion).reads(control_state, config_val, config_rs_tag_valid, config_rs_tag,
                                  head_val, head_bits);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrl ctrl("ExCtrl");
  ExCtrlDriver driver("Driver");

  ctrl.cmd_in << driver.cmd_out;
  driver.control_state << ctrl.control_state;
  driver.config_val << ctrl.config_val;
  driver.config_rs_tag_valid << ctrl.config_rs_tag_valid;
  driver.config_rs_tag << ctrl.config_rs_tag;
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
  for (int i = 0; i < 8; ++i) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.matched();
  std::printf("[EX_CTRL] %s config_reached_cmd_queue\n", ok ? "PASS" : "FAIL");
  descore::flushLog(); // flush log before exiting because trace o/p is buffered
  return ok ? 0 : 1;
}
