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
// Convert the registered state value to the FSM enum.
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
inline ConfigKind configKind(std::uint64_t rs1) {
  return static_cast<ConfigKind>(rs1 & 0x3u);
}

class ExCtrlState : public Component {
  DECLARE_COMPONENT(ExCtrlState);

 public:
  ExCtrlState(std::string name, COMPONENT_CTOR);

  Clock(clk);
  // inputs to FSM
  InputArray(bit,        head_val,  kExCtrlCmdWindow);     // cmd queue head valid bits
  InputArray(SmeshIssue, head_bits, kExCtrlCmdWindow);     // cmd queue head bits
  Input(bit,             do_config);                       // cmd(0) is config?
  InputArray(bit,        do_preloads, kExCtrlCmdWindow);   // cmd(0/1/2) is preload?
  InputArray(bit,        do_computes, kExCtrlCmdWindow);   // cmd(0/1/2) is compute?
  Input(bit,             matmul_in_progress);              // mesh reports an in-flight matmul
  Input(bit,             pending_completed_valid);         // completion block has pending completions

  Output(u8, control_state); // current FSM state

  Input(bit,             raw_hazards_are_impossible);      // no RAW hazards possible for this hardware config
  Input(bit,             raw_hazard_pre);                  // PRELOAD branch has a RAW hazard
  Input(bit,             a_should_be_fed_into_transposer); // decoder says A should start through transposer path
  Input(bit,             b_should_be_fed_into_transposer); // decoder says B should start through transposer path
  Input(bit,             d_should_be_fed_into_transposer); // decoder says D should start through transposer path
  Input(bit,             in_prop);                         // cmd(0) is COMPUTE_AND_FLIP
  Input(bit,             about_to_fire_all_rows);          // row-feed logic reports the final row-beat can fire
  Input(SmeshLocalAddr,  c_address_rs2);                   // decoder's PRELOAD output destination

  // FSM/mode
  Output(bit, performing_single_preload); // immediately signal standalone PRELOAD active (while latching perform_single_preload)
  // Config/programmed
  Output(bit, config_initialized);  // CONFIG_EX has initialized execute config registers
  Output(bit, a_transpose);         // CONFIG_EX A transpose register
  Output(bit, bd_transpose);        // CONFIG_EX B/D transpose register, TODO: decode when encoded
  Output(u8,  current_dataflow);    // execute dataflow register, TODO: decode when encoded
  Output(u8,  activation);          // CONFIG_EX activation register
  Output(u32, acc_scale);           // CONFIG_EX accumulator read scaling register
  Output(u32, a_addr_stride);       // CONFIG_EX A local-address stride
  Output(u32, c_addr_stride);       // CONFIG_EX C local-address stride
  Output(u8,  shift);               // CONFIG_EX in_shift register for mesh-control packets
  Output(bit, computing);           // any execute operation mode is currently feeding rows
  Output(bit, start_inputting_a);   // begin feeding A operand rows (drive read req & row-feed logic)
  Output(bit, start_inputting_b);   // begin feeding B operand rows (drive read req & row-feed logic)
  Output(bit, start_inputting_d);   // begin feeding D/preload operand rows  (drive read req & row-feed logic)
  Output(bit, prop);                // mesh-control propagate value
  Output(u8,  cmd_pop_count);       // number of command-window entries consumed this cycle
  // Completion
  Output(bit,             config_val);                    // FSM accepts CONFIG_EX this cycle
  Output(bit,             config_rs_tag_valid);           // val CONFIG_EX completion tag
  Output(SmeshRsTag,      config_rs_tag);                 // info to send back on completed port
  OutputArray(bit,        pending_completed_set_val, 2);  // FSM writes pending completion slots
  OutputArray(SmeshRsTag, pending_completed_set_bits, 2); // tags written into pending slots

  void update();
  void reset();

 private:
  Register(u8, control_state_reg_); // next FSM state
};

} // namespace smesh
