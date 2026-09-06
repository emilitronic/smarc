// **********************************************************************
// smesh/include/ExCtrlState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 5 2026
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

// *******
// Helpers
// *******
// Convert the registered numeric u8 state value to the FSM enum.
inline ExCtrlFsmState toFsmState(u8 value) {
  return static_cast<ExCtrlFsmState>(static_cast<std::uint8_t>(value));
}
// Read the raw rs1 operand from an execute command.
inline std::uint64_t rawRs1(const SmeshIssue& issue) {
  return static_cast<std::uint64_t>(issue.cmd.rs1);
}
// Read the raw rs2 operand from an execute command.
inline std::uint64_t rawRs2(const SmeshIssue& issue) {
  return static_cast<std::uint64_t>(issue.cmd.rs2);
}
// Decode the two low bits that select the CONFIG command kind.
inline ConfigKind configType(std::uint64_t rs1) {
  return static_cast<ConfigKind>(rs1 & 0x3u);
}

class ExCtrlState : public Component {
  DECLARE_COMPONENT(ExCtrlState);

 public:
  ExCtrlState(std::string name, COMPONENT_CTOR);

  Clock(clk);
  // inputs to FSM from cmd queue
  InputArray(bit,        head_val,  kExCtrlCmdWindow);     // cmd queue head valid bits
  InputArray(SmeshIssue, head_bits, kExCtrlCmdWindow);     // cmd queue head bits
  // inputs to FSM from cmd decoder
  Input(bit,            do_config);                       // cmd(0) is config?
  InputArray(bit,       do_preloads, kExCtrlCmdWindow);   // cmd(0/1/2) is preload?
  InputArray(bit,       do_computes, kExCtrlCmdWindow);   // cmd(0/1/2) is compute?
  Input(bit,            matmul_in_progress);              // mesh reports an in-flight matmul
  Input(bit,            raw_hazards_are_impossible);      // no RAW hazards possible for this hardware config
  Input(bit,            raw_hazard_pre);                  // PRELOAD branch has a RAW hazard
  Input(bit,            a_should_be_fed_into_transposer); // decoder says A should start through transposer path
  Input(bit,            b_should_be_fed_into_transposer); // decoder says B should start through transposer path
  Input(bit,            d_should_be_fed_into_transposer); // decoder says D should start through transposer path
  Input(SmeshLocalAddr, c_address_rs2);                   // decoder's PRELOAD output destination
  Input(bit,            in_prop);                         // cmd(0) is COMPUTE_AND_FLIP
  // input to FSM from row-feed logic
  Input(bit,            about_to_fire_all_rows);          // row-feed logic reports the final row-beat can fire
  // input to FSM form completion block
  Input(bit,            pending_completed_val);           // completion block has pending completions

  // outputs to cmd queue
  Output(u8,  cmd_pop_count); // number of command-window entries consumed this cycle

  // outputs to decoder
  Output(bit, a_transpose);         // CONFIG_EX A transpose register
  Output(bit, bd_transpose);        // CONFIG_EX B/D transpose register, TODO: decode when encoded

  // output to cntl packet
  Output(bit, performing_single_preload); // standalone PRELOAD is active this cycle (put in )
  Output(bit, performing_mul_pre);
  Output(bit, performing_single_mul);

  // output to cntl queue and its logic
  Output(u8,  control_state);             // current FSM state
  Output(bit, computing);           // any execute operation mode is currently feeding rows

  // outputs to completion block
  Output(bit,             config_val);                    // FSM accepts CONFIG_EX this cycle
  Output(bit,             config_rs_tag_val);             // val CONFIG_EX completion tag
  Output(SmeshRsTag,      config_rs_tag);                 // info to send back on completed port
  OutputArray(bit,        pending_completed_set_val, 2);  // FSM writes pending completion slots
  OutputArray(SmeshRsTag, pending_completed_set_bits, 2); // tags written into pending slots
  
  
  //---------------------------

  // Config/programmed

  Output(u8,  current_dataflow);    // execute dataflow register, TODO: decode when encoded
  Output(u8,  activation);          // CONFIG_EX activation register
  Output(u32, acc_scale);           // CONFIG_EX accumulator read scaling register
  Output(u32, a_addr_stride);       // CONFIG_EX A local-address stride
  Output(u32, c_addr_stride);       // CONFIG_EX C local-address stride
  Output(u8,  shift);               // CONFIG_EX in_shift register for mesh-control packets
  Output(u8,  ocol);                // CONFIG_IM2COL output columns
  Output(u8,  kdim2);               // CONFIG_IM2COL squared kernel dimension
  Output(u8,  krow);                // CONFIG_IM2COL kernel row
  Output(u32, channel);             // CONFIG_IM2COL input channel
  Output(u8,  weight_stride);       // CONFIG_IM2COL weight stride
  Output(bit, weight_double_bank);  // CONFIG_IM2COL double-bank mode
  Output(bit, weight_triple_bank);  // CONFIG_IM2COL triple-bank mode
  Output(u8,  row_left);            // CONFIG_IM2COL rows remaining
  Output(u32, row_turn);            // CONFIG_IM2COL row-turn count
  Output(bit, start_inputting_a);   // begin feeding A operand rows (drive read req & row-feed logic)
  Output(bit, start_inputting_b);   // begin feeding B operand rows (drive read req & row-feed logic)
  Output(bit, start_inputting_d);   // begin feeding D/preload operand rows  (drive read req & row-feed logic)
  Output(bit, prop);                // mesh-control propagate value

  void updateStartInputting();
  void update();
  void reset();

 private:
  Register(u8,  control_state_D_); // next FSM state
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

  Output(bit,   config_initialized_Q_);  // CONFIG_EX has initialized execute config registers
  Register(bit, config_initialized_D_);

  Output(bit,   perform_single_preload_Q_); // registered standalone PRELOAD mode
  Register(bit, perform_single_preload_D_);
  Output(bit,   perform_mul_pre_Q_);        // registered standalone MUL_PRE mode
  Register(bit, perform_mul_pre_D_);
  Output(bit,   perform_single_mul_Q_);     // registered standalone PRELOAD mode
  Register(bit, perform_single_mul_D_);

  Output(bit,   in_prop_flush_Q_);
  Register(bit, in_prop_flush_D_);

};

} // namespace smesh
