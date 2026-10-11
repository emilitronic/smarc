// **********************************************************************
// smesh/src/loop_conv/LoopConvCmdQueue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvCmdQueue.hpp"

namespace smesh {

LoopConvCmdQueue::LoopConvCmdQueue(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateHead).reads(state_Q_).writes(head_val, head_bits);
  UPDATE(updateReady).reads(state_Q_, head_rdy).writes(cmd_rdy);
  UPDATE(updateStorage)
      .reads(state_Q_, cmd_val, cmd_bits, cmd_rdy, head_rdy)
      .writes(state_D_);
}

// Present the oldest buffered command to the future LoopConv decoder.
void LoopConvCmdQueue::updateHead() {
  const auto state = *state_Q_;
  head_val = bit(state.count != 0);
  head_bits = state.count != 0 ? state.entries[0] : SmeshQueuedCmd{};
}

// Accept a command when there is space, including space freed this cycle.
void LoopConvCmdQueue::updateReady() {
  const auto state = *state_Q_;
  cmd_rdy = bit(state.count < kLoopConvCmdQueueLength ||
                (state.count != 0 && head_rdy == 1));
}

// Remove an accepted head, then append an accepted input for the next cycle.
void LoopConvCmdQueue::updateStorage() {
  const auto current = *state_Q_;
  auto next = current;
  const bool pop = current.count != 0 && head_rdy == 1;
  const bool push = cmd_val == 1 && cmd_rdy == 1;
  if (pop) {
    for (std::size_t i = 1; i < current.count; ++i)
      next.entries[i - 1] = current.entries[i];
    next.entries[current.count - 1] = SmeshQueuedCmd{};
    --next.count;
  }
  if (push) {
    next.entries[next.count] = *cmd_bits;
    ++next.count;
  }
  if (pop || push) {
    state_D_ = next;
    trace("loop_conv queue: push=%u pop=%u count=%u",
          unsigned(push), unsigned(pop), static_cast<unsigned>(next.count));
  }
}

void LoopConvCmdQueue::reset() {
  state_D_.reset(State{});
  head_val.reset(0);
  head_bits.reset(SmeshQueuedCmd{});
  cmd_rdy.reset(0);
}

} // namespace smesh
