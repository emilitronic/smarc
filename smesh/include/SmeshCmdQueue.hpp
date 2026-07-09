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

  FifoInput(SmeshCmd, cmd_in);
  FifoOutput(SmeshQueuedCmd, cmd_out);

  void update();
};

} // namespace smesh
