// **********************************************************************
// smesh/include/LdCtrlQueues.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstddef>

namespace smesh {

constexpr std::size_t kLdCtrlCmdQueueLength = 8;

// Buffers RS load commands until the Load FSM consumes the head command.
class LdCtrlCmdQueue : public Component {
  DECLARE_COMPONENT(LdCtrlCmdQueue);

 public:
  LdCtrlCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,        cmd_val);
  Output(bit,       cmd_rdy);
  Input(SmeshIssue, cmd_bits);

  Output(bit,        head_val);
  Output(SmeshIssue, head_bits);
  Input(bit,         head_rdy);

  void updateHeadView();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  struct State {
    std::array<SmeshIssue, kLdCtrlCmdQueueLength> entries{};
    u8 count = 0;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
