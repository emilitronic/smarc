// **********************************************************************
// smesh/include/loop_conv/LoopConvCmdQueue.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Buffers two incoming commands and holds the oldest command until head_rdy.
Like the LoopMatmul queue, a full queue can accept a replacement when its
head is consumed in the same cycle.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>

namespace smesh {

constexpr std::size_t kLoopConvCmdQueueLength = 2;

class LoopConvCmdQueue : public Component {
  DECLARE_COMPONENT(LoopConvCmdQueue);

 public:
  LoopConvCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, cmd_val);
  Input(SmeshQueuedCmd, cmd_bits);
  Output(bit, cmd_rdy);

  Output(bit, head_val);
  Output(SmeshQueuedCmd, head_bits);
  Input(bit, head_rdy);

  void updateHead();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  struct State {
    std::array<SmeshQueuedCmd, kLoopConvCmdQueueLength> entries{};
    u8 count = 0;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
