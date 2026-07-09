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
  UPDATE(update).reads(cmd_in).writes(cmd_out);
}

void SmeshCmdQueue::update() {
  if (cmd_in.empty() || cmd_out.full()) {
    return;
  }

  SmeshQueuedCmd queued{};
  queued.cmd             = cmd_in.pop();
  queued.rs_tag          = 0;
  queued.rs_tag_valid    = false;
  queued.from_mmul_loop  = false;
  queued.from_conv_loop  = false;
  cmd_out.push(queued);

  trace("cmd_queue: accepted funct=%u", static_cast<unsigned>(queued.cmd.funct));
}

} // namespace smesh
