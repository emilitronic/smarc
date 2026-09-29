// **********************************************************************
// smesh/include/SmeshCmdQueues.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 10 2026
/*
Command-path queue components.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class SmeshCmdQueue : public Component {
  DECLARE_COMPONENT(SmeshCmdQueue);

 public:
  SmeshCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, cmd_valid);
  Input(SmeshCmd, cmd_bits);
  Output(bit, cmd_ready);
  FifoOutput(SmeshCmd, cmd_out);

  void updateReady();  // computes cmd_ready from FIFO space
  void updateAccept(); // pushes cmd_bits when cmd_valid && cmd_ready
};

// Converts the command FIFO head to LoopMatmul's valid/ready input.
class SmeshLoopCmdAdapter : public Component {
  DECLARE_COMPONENT(SmeshLoopCmdAdapter);

 public:
  SmeshLoopCmdAdapter(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoInput(SmeshCmd, cmd_in);
  Output(bit, cmd_val);
  Output(SmeshQueuedCmd, cmd_bits);
  Input(bit, cmd_rdy);

  void update();
  void reset();
};

class SmeshUnrolledCmdQueue : public Component {
  DECLARE_COMPONENT(SmeshUnrolledCmdQueue);

 public:
  SmeshUnrolledCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, cmd_val);
  Input(SmeshQueuedCmd, cmd_bits);
  Output(bit, cmd_rdy);
  FifoOutput(SmeshQueuedCmd, cmd_out);

  void updateReady();
  void updateAccept();
  void reset();
};

} // namespace smesh
