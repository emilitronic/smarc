// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulCmdQueue.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>

namespace smesh {

constexpr std::size_t kLoopMatmulCmdQueueLength = 2;

// Buffers commands before LoopMatmul decodes and consumes them.
class LoopMatmulCmdQueue : public Component {
  DECLARE_COMPONENT(LoopMatmulCmdQueue);

 public:
  LoopMatmulCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,            cmd_val);
  Input(SmeshQueuedCmd, cmd_bits);
  Output(bit,           cmd_rdy);

  Output(bit,            head_val);
  Output(SmeshQueuedCmd, head_bits);
  Input(bit,             head_rdy);

  void updateHead();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  struct State {
    std::array<SmeshQueuedCmd, kLoopMatmulCmdQueueLength> entries{};
    u8 count = 0;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
