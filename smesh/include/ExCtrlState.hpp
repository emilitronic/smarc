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
#include "ExCtrlDecoder.hpp"
#include "SmeshCommand.hpp"
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

  Output(bit, config_initialized); // CONFIG_EX has initialized execute config registers
  Output(bit, a_transpose);        // CONFIG_EX A transpose register
  Output(bit, bd_transpose);       // CONFIG_EX B/D transpose register, TODO: decode when encoded
  Output(u8, current_dataflow);    // execute dataflow register, TODO: decode when encoded
  Output(u32, a_addr_stride);      // CONFIG_EX A local-address stride
  Output(u32, c_addr_stride);      // CONFIG_EX C local-address stride

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
  ExCtrlFsmState state_ = ExCtrlFsmState::WaitingForCmd; // control_state register

  bool config_initialized_ = false;
  bool a_transpose_ = false;
  bool bd_transpose_ = false;
  std::uint8_t current_dataflow_ = kExDataflowWS;
  std::uint32_t a_addr_stride_ = 1;
  std::uint32_t c_addr_stride_ = 1;

  // TODO: add later execute config registers:
  // activation
  // in_shift
  // acc_scale

  // TODO: add im2col config registers later:
  // ocol
  // kdim2
  // krow
  // channel
  // weight_stride
  // weight_double_bank
  // weight_triple_bank
  // row_left
  // row_turn
};

} // namespace smesh
