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
  Input(bit, matmul_in_progress);                      // mesh reports an in-flight matmul
  Input(bit, pending_completed_valid);                 // completion block has pending completions
  Input(bit, raw_hazards_are_impossible);              // no RAW hazards possible for this hardware config
  Input(bit, raw_hazard_pre);                          // PRELOAD branch has a RAW hazard
  Input(bit, a_should_be_fed_into_transposer);         // decoder says A should start through transposer path
  Input(bit, b_should_be_fed_into_transposer);         // decoder says B should start through transposer path
  Input(bit, d_should_be_fed_into_transposer);         // decoder says D should start through transposer path
  Input(bit, in_prop);                                 // cmd(0) is COMPUTE_AND_FLIP

  Output(u8,  control_state);       // current value of the FSM state register
  Output(bit, config_initialized);  // CONFIG_EX has initialized execute config registers
  Output(bit, a_transpose);         // CONFIG_EX A transpose register
  Output(bit, bd_transpose);        // CONFIG_EX B/D transpose register, TODO: decode when encoded
  Output(u8,  current_dataflow);    // execute dataflow register, TODO: decode when encoded
  Output(u8,  activation);          // CONFIG_EX activation register
  Output(u32, a_addr_stride);       // CONFIG_EX A local-address stride
  Output(u32, c_addr_stride);       // CONFIG_EX C local-address stride
  Output(u8,  shift);               // CONFIG_EX in_shift register for mesh-control packets
  Output(bit, config_val);          // FSM accepts/processes a CONFIG command this cycle
  Output(bit, config_rs_tag_valid); // valid bit for CONFIG completion tag
  Output(SmeshRsTag, config_rs_tag);// info to send back on completed port
  OutputArray(bit, pending_completed_set_val, 2); // FSM writes pending completion slots
  OutputArray(SmeshRsTag, pending_completed_set_bits, 2); // tags written into pending slots
  Output(bit, performing_single_preload); // immediately signal standalone PRELOAD active (while latching perform_single_preload)
  Output(bit, computing);           // any execute operation mode is currently feeding rows
  Output(bit, start_inputting_a);   // begin feeding A operand rows
  Output(bit, start_inputting_b);   // begin feeding B operand rows
  Output(bit, start_inputting_d);   // begin feeding D/preload operand rows
  Output(bit, prop);                // mesh-control propagate value
  Output(u8, cmd_pop_count);        // number of command-window entries consumed this cycle

  // TODO: add the remaining Gemmini-aligned FSM inputs as we use them:
  // Input(bit, raw_hazard_mulpre);
  // Input(bit, third_instruction_needed);
  // Input(bit, about_to_fire_all_rows);
  // Input(u8, current_dataflow);
  // Input(bit, mesh_req_fire);
  // Input(bit, mesh_req_rdy);
  // Output(bit, performing_mul_pre);
  // Output(bit, performing_single_mul);

  void update();
  void reset();

 private:
  ExCtrlFsmState state_ = ExCtrlFsmState::WaitingForCmd; // control_state register

  bool config_initialized_     = false;
  bool a_transpose_            = false;
  bool bd_transpose_           = false;
  bool perform_single_preload_ = false; // denote standalone PRELOAD mode
  bool in_prop_flush_          = false;
  std::uint8_t current_dataflow_ = kExDataflowWS;
  std::uint8_t activation_ = 0;
  std::uint8_t in_shift_        = 0;
  std::uint32_t a_addr_stride_ = 1;
  std::uint32_t c_addr_stride_ = 1;

  // TODO: add later:

  // command mode registers:
  // perform_single_preload (done)
  // perform_single_mul
  // perform_mul_pre

  // programmed execution settings:
  // activation
  // acc_scale
  // a_transpose
  // bd_transpose
  // config_initialized
  // a_addr_stride
  // c_addr_stride

  // TODO: add im2col config registers settings:
  // ocol
  // orow
  // krow
  // weight_stride
  // channel
  // row_turn
  // row_left
  // kdim2
  // weight_double_bank
  // weight_triple_bank

  // TODO: other
  // in_prop_flush (started)
};

} // namespace smesh
