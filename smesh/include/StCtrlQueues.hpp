// **********************************************************************
// smesh/include/StCtrlQueues.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
/*
Store-controller command queue components.

The queue holds the short command window presented to the store decoder and
FSM.  StoreController uses two entries in the current model.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstddef>

namespace smesh {

constexpr std::size_t kStCtrlCmdQueueLength = 2;

class StCtrlCmdQueue : public Component {
  DECLARE_COMPONENT(StCtrlCmdQueue);

 public:
  StCtrlCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, cmd_val);
  Output(bit, cmd_rdy);
  Input(SmeshIssue, cmd_bits);

  Output(bit, head_val);          // the front command is valid
  Output(SmeshIssue, head_bits);  // the front command, when valid
  Input(bit, head_rdy);           // the FSM can consume the front command

  void updateHeadView();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  struct State {
    std::array<SmeshIssue, kStCtrlCmdQueueLength> entries{};
    u8 count = 0;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
