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
  InputArray(bit,        head_val,  kExCtrlCmdWindow);     // cmd queue head valid bits
  InputArray(SmeshIssue, head_bits, kExCtrlCmdWindow);     // cmd queue head bits
  Input(bit,             do_config);                       // cmd(0) is config?
  InputArray(bit,        do_preloads, kExCtrlCmdWindow);   // cmd(0/1/2) is preload?
  InputArray(bit,        do_computes, kExCtrlCmdWindow);   // cmd(0/1/2) is compute?
  Input(bit,             matmul_in_progress);              // mesh reports an in-flight matmul
  Input(bit,             pending_completed_valid);         // completion block has pending completions

  Output(u8, control_state); // current FSM state

  void update();
  void reset();

 private:
  Register(u8, control_state_reg_); // next FSM state
};

} // namespace smesh
