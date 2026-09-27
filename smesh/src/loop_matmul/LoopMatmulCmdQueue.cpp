// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulCmdQueue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026

#include "LoopMatmulCmdQueue.hpp"

namespace smesh {

LoopMatmulCmdQueue::LoopMatmulCmdQueue(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateHead).reads(state_Q_).writes(head_val, head_bits);
  UPDATE(updateReady).reads(state_Q_, head_rdy).writes(cmd_rdy);
  UPDATE(updateStorage).reads(state_Q_, cmd_val, cmd_bits, cmd_rdy, head_rdy).writes(state_D_);
}

void LoopMatmulCmdQueue::updateHead() {
  const auto state = *state_Q_;
  head_val  = bit(state.count != 0);
  head_bits = state.count != 0 ? state.entries[0] : SmeshQueuedCmd{};
}

void LoopMatmulCmdQueue::updateReady() {
  const auto state = *state_Q_;
  cmd_rdy = bit(state.count < kLoopMatmulCmdQueueLength ||
                (state.count != 0 && head_rdy == 1));
}

void LoopMatmulCmdQueue::updateStorage() {
  const auto current = *state_Q_;
  auto next = current;
  if (current.count != 0 && head_rdy == 1) {
    for (std::size_t i = 1; i < current.count; ++i) {
      next.entries[i - 1] = current.entries[i];
    }
    next.entries[current.count - 1] = SmeshQueuedCmd{};
    --next.count;
  }
  if (cmd_val == 1 && cmd_rdy == 1) {
    next.entries[next.count] = *cmd_bits;
    ++next.count;
  }
  state_D_ = next;
}

void LoopMatmulCmdQueue::reset() {
  state_D_.reset(State{});
  head_val.reset(0);
  head_bits.reset(SmeshQueuedCmd{});
  cmd_rdy.reset(0);
}

} // namespace smesh
