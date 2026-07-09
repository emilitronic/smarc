// **********************************************************************
// smesh/src/SmeshCmdQueue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Command ingress queue implementation.
*/

#include "SmeshCmdQueue.hpp"

namespace smesh {

SmeshCmdQueue::SmeshCmdQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(cmd_ready);
  UPDATE(updateAccept).reads(cmd_valid, cmd_bits).writes(cmd_out);
}

void SmeshCmdQueue::updateReady() {
  cmd_ready = bit(!cmd_out.full());
}

void SmeshCmdQueue::updateAccept() {
  if (cmd_out.full() || cmd_valid == 0) {
    return;
  }

  const auto cmd = *cmd_bits;
  cmd_out.push(cmd);

  trace("cmd_queue: accepted funct=%u", static_cast<unsigned>(cmd.funct));
}

} // namespace smesh
