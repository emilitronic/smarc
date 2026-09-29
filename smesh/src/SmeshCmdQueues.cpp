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

SmeshLoopCmdAdapter::SmeshLoopCmdAdapter(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_in, cmd_rdy).writes(cmd_val, cmd_bits);
}

void SmeshLoopCmdAdapter::update() {
  cmd_val = bit(!cmd_in.empty());
  cmd_bits = SmeshQueuedCmd{};
  if (cmd_in.empty()) return;

  cmd_bits = SmeshQueuedCmd{cmd_in.peek()};
  if (cmd_rdy == 1) cmd_in.pop();
}

void SmeshLoopCmdAdapter::reset() {
  cmd_val.reset(0);
  cmd_bits.reset(SmeshQueuedCmd{});
}

SmeshUnrolledCmdQueue::SmeshUnrolledCmdQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(cmd_rdy);
  UPDATE(updateAccept).reads(cmd_val, cmd_bits).writes(cmd_out);
}

void SmeshUnrolledCmdQueue::updateReady() {
  cmd_rdy = bit(!cmd_out.full());
}

void SmeshUnrolledCmdQueue::updateAccept() {
  if (cmd_val == 0 || cmd_out.full()) return;

  const auto cmd = *cmd_bits;
  cmd_out.push(cmd);

  trace("unrolled_cmd_queue: accepted funct=%u", static_cast<unsigned>(cmd.cmd.funct));
}

void SmeshUnrolledCmdQueue::reset() {
  cmd_rdy.reset(0);
}

} // namespace smesh
