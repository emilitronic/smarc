// **********************************************************************
// smesh/include/ExCtrlState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Central execute-controller FSM state holder.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlQueues.hpp"
#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

enum class ExCtrlFsmState : std::uint8_t {
  WaitingForCmd = 0,
  Compute       = 1,
  Flush         = 2,
  Flushing      = 3,
};

class ExCtrlState : public Component {
  DECLARE_COMPONENT(ExCtrlState);

 public:
  ExCtrlState(std::string name, COMPONENT_CTOR);

  Clock(clk);
  // inputs to FSM
  InputArray(bit, head_val, kExCtrlCmdWindow);         // cmd queue head valid bits
  InputArray(SmeshIssue, head_bits, kExCtrlCmdWindow); // cmd queue head bits
  Input(bit, do_config);                               // cmd(0) is config?
  InputArray(bit, do_preloads, kExCtrlCmdWindow);      // cmd(0/1/2) is preload?
  InputArray(bit, do_computes, kExCtrlCmdWindow);      // cmd(0/1/2) is compute?

  // TODO: add the remaining Gemmini-aligned FSM inputs as we use them:
  // Input(bit, matmul_in_progress);
  // Input(bit, pending_completed_valid);
  // Input(bit, raw_hazards_are_impossible);
  // Input(bit, raw_hazard_pre);
  // Input(bit, raw_hazard_mulpre);
  // Input(bit, third_instruction_needed);
  // Input(bit, about_to_fire_all_rows);
  // Input(u8, current_dataflow);
  // Input(bit, mesh_req_fire);
  // Input(bit, mesh_req_ready);

  void update();
  void reset();

 private:
  ExCtrlFsmState state_ = ExCtrlFsmState::WaitingForCmd;
};

} // namespace smesh
