// **********************************************************************
// smesh/src/tb_ex_ctrl_state_mul_pre.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 16 2026
/*
Focused ExCtrlState2 COMPUTE + PRELOAD branch test.  A focused FSM test.

cmake --build build --target tb_ex_ctrl_state_mul_pre -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl_state_mul_pre -trace '*'/mul_pre_state_
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlState2.hpp"

#include <cstdio>
#include <cstdint>

namespace {

constexpr smesh::SmeshRsTag kComputeTag = 9;
constexpr smesh::SmeshRsTag kPreloadTag = 10;

const char* stateName(std::uint8_t state) {
  switch (static_cast<smesh::ExCtrlFsmState>(state)) {
    case smesh::ExCtrlFsmState::WaitingForCmd: return "WAIT";
    case smesh::ExCtrlFsmState::Compute:       return "COMP";
    case smesh::ExCtrlFsmState::Flush:         return "FLUS";
    case smesh::ExCtrlFsmState::Flushing:      return "FLNG";
  }
  return "????";
}

} // namespace

class MulPreStateDriver : public Component {
  DECLARE_COMPONENT(MulPreStateDriver);

 public:
  MulPreStateDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  OutputArray(bit, head_val, smesh::kExCtrlCmdWindow);
  OutputArray(smesh::SmeshIssue, head_bits, smesh::kExCtrlCmdWindow);
  Output(bit, do_config);
  OutputArray(bit, do_preloads, smesh::kExCtrlCmdWindow);
  OutputArray(bit, do_computes, smesh::kExCtrlCmdWindow);
  Output(bit, matmul_in_progress);
  Output(bit, pending_completed_val);
  Output(bit, raw_hazards_are_impossible);
  Output(bit, raw_hazard_pre);
  Output(bit, raw_hazard_mulpre);
  Output(bit, third_instruction_needed);
  Output(bit, a_should_be_fed_into_transposer);
  Output(bit, b_should_be_fed_into_transposer);
  Output(bit, in_prop);
  Output(bit, about_to_fire_all_rows);
  Output(smesh::SmeshLocalAddr, c_address_rs2);
  Output(bit, mesh_req_fire);
  Output(bit, mesh_req_rdy);
  Output(u32, cycle);

  void update();
  void reset();

 private:
  Register(u32, cycle_D_);
};

MulPreStateDriver::MulPreStateDriver(std::string /*name*/, IMPL_CTOR) {
  cycle <= cycle_D_;

  UPDATE(update)
      .reads(cycle)
      .writes(head_val, head_bits, do_config, do_preloads, do_computes)
      .writes(matmul_in_progress, pending_completed_val,
              raw_hazards_are_impossible, raw_hazard_pre, raw_hazard_mulpre,
              third_instruction_needed)
      .writes(a_should_be_fed_into_transposer, b_should_be_fed_into_transposer,
              in_prop, about_to_fire_all_rows, c_address_rs2,
              mesh_req_fire, mesh_req_rdy, cycle_D_);
}

void MulPreStateDriver::update() {
  const auto now = static_cast<std::uint32_t>(*cycle);

  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    head_val[i] = 0;
    head_bits[i] = smesh::SmeshIssue{};
    do_preloads[i] = 0;
    do_computes[i] = 0;
  }

  // Present COMPUTE in slot 0 and PRELOAD in slot 1 until the final row-beat.
  if (now <= 2) {
    smesh::SmeshIssue compute{};
    compute.rs_tag_valid = 1;
    compute.rs_tag = kComputeTag;
    compute.cmd.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::ComputeFlip);

    smesh::SmeshIssue preload{};
    preload.rs_tag_valid = 1;
    preload.rs_tag = kPreloadTag;
    preload.cmd.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::Preload);

    head_val[0] = 1;
    head_bits[0] = compute;
    do_computes[0] = 1;
    head_val[1] = 1;
    head_bits[1] = preload;
    do_preloads[1] = 1;
  }

  do_config = 0;
  matmul_in_progress = 0;
  pending_completed_val = 0;
  raw_hazards_are_impossible = 0;
  raw_hazard_pre = 0;
  raw_hazard_mulpre = 0;
  third_instruction_needed = 0;
  a_should_be_fed_into_transposer = 0;
  b_should_be_fed_into_transposer = 0;
  in_prop = 1;
  about_to_fire_all_rows = bit(now == 2);
  c_address_rs2 = smesh::makeAccAddr(12);
  mesh_req_fire = 0;
  mesh_req_rdy = 1;

  cycle_D_ = now + 1;
}

void MulPreStateDriver::reset() {
  cycle_D_.reset(0);
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    head_val[i].reset(0);
    head_bits[i].reset(smesh::SmeshIssue{});
    do_preloads[i].reset(0);
    do_computes[i].reset(0);
  }
  do_config.reset(0);
  matmul_in_progress.reset(0);
  pending_completed_val.reset(0);
  raw_hazards_are_impossible.reset(0);
  raw_hazard_pre.reset(0);
  raw_hazard_mulpre.reset(0);
  third_instruction_needed.reset(0);
  a_should_be_fed_into_transposer.reset(0);
  b_should_be_fed_into_transposer.reset(0);
  in_prop.reset(0);
  about_to_fire_all_rows.reset(0);
  c_address_rs2.reset(smesh::SmeshLocalAddr{});
  mesh_req_fire.reset(0);
  mesh_req_rdy.reset(0);
}

class MulPreStateMonitor : public Component {
  DECLARE_COMPONENT(MulPreStateMonitor);

 public:
  MulPreStateMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(u32, cycle);
  Input(u8, control_state);
  Input(bit, performing_mul_pre);
  Input(bit, start_inputting_a);
  Input(bit, start_inputting_b);
  Input(bit, start_inputting_d);
  Input(bit, computing);
  Input(bit, prop);
  Input(u8, cmd_pop_count);
  InputArray(bit, pending_completed_set_val, 2);
  InputArray(smesh::SmeshRsTag, pending_completed_set_bits, 2);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool done_ = false;
  bool passed_ = true;
};

TraceKey(mul_pre_state_);

MulPreStateMonitor::MulPreStateMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(cycle, control_state, performing_mul_pre,
             start_inputting_a, start_inputting_b, start_inputting_d,
             computing, prop)
      .reads(cmd_pop_count)
      .reads(pending_completed_set_val, pending_completed_set_bits);
}

void MulPreStateMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  const auto now = static_cast<std::uint32_t>(*cycle);
  const auto state = static_cast<std::uint8_t>(*control_state);
  bool expected = true;

  if (now == 0) {
    expected = state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::WaitingForCmd) &&
               performing_mul_pre == 1 && computing == 1 &&
               start_inputting_a == 1 && start_inputting_b == 1 && start_inputting_d == 1 &&
               prop == 1 && *cmd_pop_count == 0;
  } else if (now == 1) {
    expected = state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::Compute) &&
               performing_mul_pre == 1 && computing == 1 &&
               start_inputting_a == 1 && start_inputting_b == 1 && start_inputting_d == 1 &&
               *cmd_pop_count == 0;
  } else if (now == 2) {
    expected = state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::Compute) &&
               performing_mul_pre == 1 && computing == 1 &&
               start_inputting_a == 1 && start_inputting_b == 1 && start_inputting_d == 1 &&
               *cmd_pop_count == 2 &&
               pending_completed_set_val[0] == 1 &&
               *pending_completed_set_bits[0] == kComputeTag &&
               pending_completed_set_val[1] == 0;
  } else if (now == 3) {
    expected = state == static_cast<std::uint8_t>(smesh::ExCtrlFsmState::WaitingForCmd) &&
               performing_mul_pre == 0 && computing == 0 &&
               start_inputting_a == 0 && start_inputting_b == 0 && start_inputting_d == 0 &&
               *cmd_pop_count == 0;
    done_ = true;
  }

  s_trace(mul_pre_state_,
          "cycle=%u state=%s mulpre=%u start{%u%u%u} computing=%u prop=%u pop=%u pending{%u:%u,%u:%u} %s\n",
          static_cast<unsigned>(now), stateName(state),
          static_cast<unsigned>(performing_mul_pre != 0),
          static_cast<unsigned>(start_inputting_a != 0),
          static_cast<unsigned>(start_inputting_b != 0),
          static_cast<unsigned>(start_inputting_d != 0),
          static_cast<unsigned>(computing != 0),
          static_cast<unsigned>(prop != 0),
          static_cast<unsigned>(*cmd_pop_count),
          static_cast<unsigned>(pending_completed_set_val[0] != 0),
          static_cast<unsigned>(*pending_completed_set_bits[0]),
          static_cast<unsigned>(pending_completed_set_val[1] != 0),
          static_cast<unsigned>(*pending_completed_set_bits[1]),
          expected ? "OK" : "ERROR");

  passed_ = passed_ && expected;
}

void MulPreStateMonitor::reset() {
  done_ = false;
  passed_ = true;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  MulPreStateDriver driver("Driver");
  smesh::ExCtrlState2 state("State");
  MulPreStateMonitor monitor("Monitor");

  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    state.head_val[i] << driver.head_val[i];
    state.head_bits[i] << driver.head_bits[i];
    state.do_preloads[i] << driver.do_preloads[i];
    state.do_computes[i] << driver.do_computes[i];
  }
  state.do_config << driver.do_config;
  state.matmul_in_progress << driver.matmul_in_progress;
  state.pending_completed_val << driver.pending_completed_val;
  state.raw_hazards_are_impossible << driver.raw_hazards_are_impossible;
  state.raw_hazard_pre << driver.raw_hazard_pre;
  state.raw_hazard_mulpre << driver.raw_hazard_mulpre;
  state.third_instruction_needed << driver.third_instruction_needed;
  state.a_should_be_fed_into_transposer << driver.a_should_be_fed_into_transposer;
  state.b_should_be_fed_into_transposer << driver.b_should_be_fed_into_transposer;
  state.in_prop << driver.in_prop;
  state.about_to_fire_all_rows << driver.about_to_fire_all_rows;
  state.c_address_rs2 << driver.c_address_rs2;
  state.mesh_req_fire << driver.mesh_req_fire;
  state.mesh_req_rdy << driver.mesh_req_rdy;

  monitor.cycle << driver.cycle;
  monitor.control_state << state.control_state;
  monitor.performing_mul_pre << state.performing_mul_pre;
  monitor.start_inputting_a << state.start_inputting_a;
  monitor.start_inputting_b << state.start_inputting_b;
  monitor.start_inputting_d << state.start_inputting_d;
  monitor.computing << state.computing;
  monitor.prop << state.prop;
  monitor.cmd_pop_count << state.cmd_pop_count;
  for (std::size_t i = 0; i < 2; ++i) {
    monitor.pending_completed_set_val[i] << state.pending_completed_set_val[i];
    monitor.pending_completed_set_bits[i] << state.pending_completed_set_bits[i];
  }

  Clock clk;
  driver.clk << clk;
  state.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 6 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_STATE_MUL_PRE] %s compute_preload_branch\n",
              ok ? "PASS" : "FAIL");
  descore::flushLog();
  return ok ? 0 : 1;
}
