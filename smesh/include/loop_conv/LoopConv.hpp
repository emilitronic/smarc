// **********************************************************************
// smesh/include/loop_conv/LoopConv.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
First construction of LoopConv: owns and connects its input command queue.
The queue head is exposed for now; the future decoder/router will consume it
inside this component. Configuration decoding and command generation are
not implemented yet.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class LoopConvCmdQueue;

class LoopConv : public Component {
  DECLARE_COMPONENT(LoopConv);

 public:
  LoopConv(std::string name, COMPONENT_CTOR);
  ~LoopConv() override;

  Clock(clk);

  Input(bit, in_val);
  Input(SmeshQueuedCmd, in_bits);
  Output(bit, in_rdy);

  // Temporary queue-head interface, before the internal decoder is added.
  Output(bit, head_val);
  Output(SmeshQueuedCmd, head_bits);
  Input(bit, head_rdy);
  Output(bit, busy);

 private:
  LoopConvCmdQueue* cmd_queue_ = nullptr;
};

} // namespace smesh
