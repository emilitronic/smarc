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

  FifoInput(SmeshIssue, cmd_in);

  Output(bit, head_val);          // the front command is valid
  Output(SmeshIssue, head_bits);  // the front command, when valid
  Input(bit, head_rdy);           // the FSM can consume the front command

  void updateHeadView();
  void updateStorage();
  void reset();

 private:
  std::array<SmeshIssue, kStCtrlCmdQueueLength> entries_{};
  std::size_t count_ = 0;
};

} // namespace smesh
