// **********************************************************************
// smesh/src/SmeshCmdQueues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 10 2026
/*
Command-path queue implementations.
*/

#include "SmeshCmdQueues.hpp"

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

SmeshUnrolledCmdQueue::SmeshUnrolledCmdQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_in).writes(cmd_out);
}

void SmeshUnrolledCmdQueue::update() {
  if (cmd_in.empty() || cmd_out.full()) {
    return;
  }

  const auto cmd = cmd_in.pop();
  cmd_out.push(cmd);

  trace("unrolled_cmd_queue: accepted funct=%u", static_cast<unsigned>(cmd.funct));
}

} // namespace smesh
