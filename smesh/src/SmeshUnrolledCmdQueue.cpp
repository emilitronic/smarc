// **********************************************************************
// smesh/src/SmeshUnrolledCmdQueue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Queue for primitive commands after loop-command expansion or bypass.
*/

#include "SmeshUnrolledCmdQueue.hpp"

namespace smesh {

SmeshUnrolledCmdQueue::SmeshUnrolledCmdQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_in).writes(cmd_out);
}

void SmeshUnrolledCmdQueue::update() {
  if (cmd_in.empty() || cmd_out.full()) {
    return;
  }

  const auto cmd = cmd_in.pop();
  cmd_out.push(cmd);

  trace("unrolled_cmd_queue: accepted funct=%u",
        static_cast<unsigned>(cmd.cmd.funct));
}

} // namespace smesh
