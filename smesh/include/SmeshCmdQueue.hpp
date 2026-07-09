// **********************************************************************
// smesh/include/SmeshCmdQueue.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Command ingress queue.
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

} // namespace smesh
