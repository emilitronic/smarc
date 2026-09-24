// **********************************************************************
// smesh/src/StCtrlQueues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
/*
Store-controller command queue implementation.
*/

#include "StCtrlQueues.hpp"

namespace smesh {

StCtrlCmdQueue::StCtrlCmdQueue(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateHeadView).reads(state_Q_).writes(head_val, head_bits);
  UPDATE(updateReady).reads(state_Q_, head_rdy).writes(cmd_rdy);
  UPDATE(updateStorage).reads(state_Q_, cmd_val, cmd_rdy, cmd_bits, head_rdy).writes(state_D_);
}

// Present the command that the decoder and FSM should inspect this cycle.
void StCtrlCmdQueue::updateHeadView() {
  const auto current = *state_Q_;
  head_val = bit(current.count != 0);
  head_bits = current.count != 0 ? current.entries[0] : SmeshIssue{};
}

void StCtrlCmdQueue::updateReady() {
  const auto current = *state_Q_;
  cmd_rdy = bit(current.count < kStCtrlCmdQueueLength ||
                (current.count != 0 && head_rdy == 1));
}

// Pop the processed command, then accept a new one on the input handshake.
void StCtrlCmdQueue::updateStorage() {
  const auto current = *state_Q_;
  auto next = current;
  const bool head_fire = current.count != 0 && head_rdy == 1;
  if (head_fire) {
    const auto popped = current.entries[0];
    for (std::size_t i = 1; i < current.count; ++i) {
      next.entries[i - 1] = current.entries[i];
    }
    next.entries[current.count - 1] = SmeshIssue{};
    --next.count;

    trace("st_cmd_queue: popped tag=%u funct=%u",
          static_cast<unsigned>(popped.rs_tag),
          static_cast<unsigned>(popped.cmd.funct));
  }

  if (cmd_val == 1 && cmd_rdy == 1) {
    const auto issue = *cmd_bits;
    next.entries[next.count] = issue;
    ++next.count;

    trace("st_cmd_queue: accepted tag=%u funct=%u",
          static_cast<unsigned>(issue.rs_tag),
          static_cast<unsigned>(issue.cmd.funct));
  }
  state_D_ = next;
}

void StCtrlCmdQueue::reset() {
  state_D_.reset(State{});
  cmd_rdy.reset(0);
  head_val.reset(0);
  head_bits.reset(SmeshIssue{});
}

} // namespace smesh
