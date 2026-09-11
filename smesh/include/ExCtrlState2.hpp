// **********************************************************************
// smesh/include/ExCtrlState2.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 6 2026
/*
Alternative ExCtrl FSM organization which selects each accepted action once.
Takes CONFIG cmd to update configuration registers and subsequently pops it from cmd queue.
In COMPUTE mode, selects which op (SINGLE_PRELOAD, MUL_PRE, or SINGLE_MUL) is active
and asserts required start_inputting_a/b/d signals while rows are being fed.
When all rows have been issued, it pops, the cmd, records any pending completion and returns
to WAITING_FOR_CMD.  
If a FLUSH command is accepted, it enters FLUSHING state and waits for mesh_req_rdy
to return to WAITING_FOR_CMD
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlState.hpp"

namespace smesh {

class ExCtrlState2 : public Component {
  DECLARE_COMPONENT(ExCtrlState2);

 public:
  ExCtrlState2(std::string name, COMPONENT_CTOR);

  Clock(clk);

  InputArray(bit,        head_val,  kExCtrlCmdWindow);
  InputArray(SmeshIssue, head_bits, kExCtrlCmdWindow);
  Input(bit,             do_config);
  InputArray(bit,        do_preloads, kExCtrlCmdWindow);
  InputArray(bit,        do_computes, kExCtrlCmdWindow);
  Input(bit,             matmul_in_progress);
  Input(bit,             raw_hazards_are_impossible);
  Input(bit,             raw_hazard_pre);
  Input(bit,             raw_hazard_mulpre);
  Input(bit,             third_instruction_needed);
  Input(bit,             a_should_be_fed_into_transposer);
  Input(bit,             b_should_be_fed_into_transposer);
  Input(bit,             d_should_be_fed_into_transposer);
  Input(SmeshLocalAddr,  c_address_rs2);
  Input(bit,             in_prop);
  Input(bit,             about_to_fire_all_rows);
  Input(bit,             pending_completed_val);
  Input(bit,             mesh_req_fire);
  Input(bit,             mesh_req_rdy);

  Output(u8, cmd_pop_count);

  Output(bit, a_transpose);
  Output(bit, bd_transpose);

  Output(bit, performing_single_preload);
  Output(bit, performing_mul_pre);
  Output(bit, performing_single_mul);

  Output(u8,  control_state);
  Output(bit, computing);

  Output(bit,             config_val);
  Output(bit,             config_rs_tag_val);
  Output(SmeshRsTag,      config_rs_tag);
  OutputArray(bit,        pending_completed_set_val, 2);
  OutputArray(SmeshRsTag, pending_completed_set_bits, 2);

  Output(u8,  current_dataflow);
  Output(u8,  activation);
  Output(u32, acc_scale);
  Output(u32, a_addr_stride);
  Output(u32, c_addr_stride);
  Output(u8,  shift);
  Output(u8,  ocol);
  Output(u8,  kdim2);
  Output(u8,  krow);
  Output(u32, channel);
  Output(u8,  weight_stride);
  Output(bit, weight_double_bank);
  Output(bit, weight_triple_bank);
  Output(u8,  row_left);
  Output(u32, row_turn);
  Output(bit, start_inputting_a); // current op requires A stream supplied to mesh
  Output(bit, start_inputting_b);
  Output(bit, start_inputting_d);
  Output(bit, prop);

  void updateCmdAcceptanceAndOutputs();
  void updateState();
  void reset();

 private:
  // Combinational decisions naming the WaitingForCmd branch selected now.
  Output(bit, accepting_config_);
  Output(bit, accepting_single_preload_);
  Output(bit, accepting_mul_pre_);
  Output(bit, accepting_single_mul_);
  Output(bit, starting_flush_);

  Register(u8,  control_state_D_);
  Register(u8,  in_shift_D_);
  Register(u8,  activation_D_);
  Register(u32, acc_scale_D_);
  Register(bit, a_transpose_D_);
  Register(bit, bd_transpose_D_);
  Register(u8,  current_dataflow_D_);
  Register(u32, a_addr_stride_D_);
  Register(u32, c_addr_stride_D_);
  Register(u8,  ocol_D_);
  Register(u8,  kdim2_D_);
  Register(u8,  krow_D_);
  Register(u32, channel_D_);
  Register(u8,  weight_stride_D_);
  Register(bit, weight_double_bank_D_);
  Register(bit, weight_triple_bank_D_);
  Register(u8,  row_left_D_);
  Register(u32, row_turn_D_);

  Output(bit,   config_initialized_Q_);
  Register(bit, config_initialized_D_);

  Output(bit,   perform_single_preload_Q_);
  Register(bit, perform_single_preload_D_);
  Output(bit,   perform_mul_pre_Q_);
  Register(bit, perform_mul_pre_D_);
  Output(bit,   perform_single_mul_Q_);
  Register(bit, perform_single_mul_D_);

  Output(bit,   in_prop_flush_Q_);
  Register(bit, in_prop_flush_D_);
};

} // namespace smesh
