// **********************************************************************
// smesh/src/LdCtrlQueues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrlQueues.hpp"

namespace smesh {

LdCtrlCmdQueue::LdCtrlCmdQueue(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateHeadView).reads(state_Q_).writes(head_val, head_bits);
  UPDATE(updateReady).reads(state_Q_, head_rdy).writes(cmd_rdy);
  UPDATE(updateStorage).reads(state_Q_, cmd_val, cmd_rdy, cmd_bits, head_rdy).writes(state_D_);
}

void LdCtrlCmdQueue::updateHeadView() {
  const auto current = *state_Q_;
  head_val = bit(current.count != 0);
  head_bits = current.count != 0 ? current.entries[0] : SmeshIssue{};
}

void LdCtrlCmdQueue::updateReady() {
  const auto current = *state_Q_;
  cmd_rdy = bit(current.count < kLdCtrlCmdQueueLength ||
                (current.count != 0 && head_rdy == 1));
}

// The head advances only on its handshake; a full queue may pop and push together.
void LdCtrlCmdQueue::updateStorage() {
  const auto current = *state_Q_;
  auto next = current;

  if (current.count != 0 && head_rdy == 1) {
    for (std::size_t i = 1; i < current.count; ++i) {
      next.entries[i - 1] = current.entries[i];
    }
    next.entries[current.count - 1] = SmeshIssue{};
    --next.count;
  }

  if (cmd_val == 1 && cmd_rdy == 1) {
    next.entries[next.count] = *cmd_bits;
    ++next.count;
  }

  state_D_ = next;
}

void LdCtrlCmdQueue::reset() {
  state_D_.reset(State{});
  cmd_rdy.reset(0);
  head_val.reset(0);
  head_bits.reset(SmeshIssue{});
}

} // namespace smesh
