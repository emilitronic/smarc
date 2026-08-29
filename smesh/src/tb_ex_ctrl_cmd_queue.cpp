// **********************************************************************
// smesh/src/tb_ex_ctrl_cmd_queue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 28 2026
/*
Focused ExCtrlCmdQueue CONFIG enqueue test.

cmake --build build --target tb_ex_ctrl_cmd_queue -j >/dev/null 2>&1
cmake --build build --target tb_ex_ctrl_cmd_queue -j 
./build/smesh/tb_ex_ctrl_cmd_queue
*/
#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlQueues.hpp"
#include "SmeshCommand.hpp"

#include <cstdio>

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
  return "????";
}

const char* commandName(bool valid, std::uint32_t funct) {
  return valid ? functName(funct) : "---";
}

} // namespace

class CmdQueueDriver : public Component {
  DECLARE_COMPONENT(CmdQueueDriver);

 public:
  CmdQueueDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshIssue, cmd_out);
  Output(u8, pop_count);
  InputArray(bit, head_val, smesh::kExCtrlCmdWindow);
  InputArray(smesh::SmeshIssue, head_bits, smesh::kExCtrlCmdWindow);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  int cycle_ = 0;
  bool sent_ = false;
  bool done_ = false;
  bool passed_ = false;
};

CmdQueueDriver::CmdQueueDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(head_val, head_bits).writes(cmd_out, pop_count);
}

void CmdQueueDriver::update() {
  pop_count = 0;

  if (Sim::state == Sim::SimResetting) {
    return;
  }

  std::printf("[c%03d] h0{v=%u t=%03u c=%4s} h1{v=%u t=%03u c=%4s} h2{v=%u t=%03u c=%4s}\n",
              cycle_,
              static_cast<unsigned>(head_val[0]),
              static_cast<unsigned>(head_bits[0]->rs_tag),
              commandName(head_val[0] != 0, head_bits[0]->cmd.funct),
              static_cast<unsigned>(head_val[1]),
              static_cast<unsigned>(head_bits[1]->rs_tag),
              commandName(head_val[1] != 0, head_bits[1]->cmd.funct),
              static_cast<unsigned>(head_val[2]),
              static_cast<unsigned>(head_bits[2]->rs_tag),
              commandName(head_val[2] != 0, head_bits[2]->cmd.funct));

  if (!sent_ && !cmd_out.full()) {
    smesh::SmeshIssue issue{};
    issue.rs_tag_valid = 1;
    issue.rs_tag = 7;
    issue.cmd.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::Config);
    issue.cmd.rs1 = smesh::packConfigExecuteRs1(1);
    issue.cmd.rs2 = smesh::packConfigExecuteRs2(1);
    cmd_out.push(issue);
    sent_ = true;
    return;
  }

  if (sent_ && head_val[0] != 0) {
    const auto& issue = *head_bits[0];
    passed_ = issue.rs_tag_valid != 0 &&
              issue.rs_tag == 7 &&
              issue.cmd.funct == static_cast<std::uint32_t>(smesh::SmeshFunct::Config) &&
              issue.cmd.rs1 == smesh::packConfigExecuteRs1(1) &&
              issue.cmd.rs2 == smesh::packConfigExecuteRs2(1) &&
              head_val[1] == 0;
    done_ = true;
  }

  ++cycle_;
}

void CmdQueueDriver::reset() {
  sent_ = false;
  done_ = false;
  passed_ = false;
  cycle_ = 0;
  pop_count.reset(0);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrlCmdQueue queue("ExCtrlCmdQueue");
  CmdQueueDriver driver("Driver");
  queue.setTrace("");

  queue.cmd_in << driver.cmd_out;
  queue.pop_count << driver.pop_count;
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    driver.head_val[i] << queue.head_val[i];
    driver.head_bits[i] << queue.head_bits[i];
  }
  queue.cmd_in.setDelay(1);

  Clock clk;
  queue.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8 && !driver.done(); ++i) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.passed();
  std::printf("[EX_CTRL_CMD_QUEUE] %s config_enqueue\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
