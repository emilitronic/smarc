// **********************************************************************
// smesh/include/loop_conv/LoopConv.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Buffers incoming commands and passes ordinary commands to the output.
LOOP_CONV_WS configuration and launch commands remain at the queue head
until the internal decoder and loop-bank storage are implemented.
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

  Input(bit,            in_val);
  Input(SmeshQueuedCmd, in_bits);
  Output(bit,           in_rdy);

  Output(bit,            out_val);
  Output(SmeshQueuedCmd, out_bits);
  Input(bit,             out_rdy);
  Output(bit,            busy);

  void updateClassify();
  void updateRouting();
  void reset();

 private:
  LoopConvCmdQueue* cmd_queue_ = nullptr;

  Input(bit,            head_val_);
  Input(SmeshQueuedCmd, head_bits_);
  Output(bit,           head_rdy_);
  Output(bit,           is_loop_run_cmd_);
  Output(bit,           is_loop_config_cmd_);
  Output(bit,           is_loop_cmd_);
};

} // namespace smesh
