// **********************************************************************
// smesh/include/SmeshUnrolledCmdQueue.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Queue for primitive commands after loop-command expansion or bypass.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class SmeshUnrolledCmdQueue : public Component {
  DECLARE_COMPONENT(SmeshUnrolledCmdQueue);

 public:
  SmeshUnrolledCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshQueuedCmd, cmd_in);
  FifoOutput(SmeshQueuedCmd, cmd_out);

  void update();
};

} // namespace smesh
