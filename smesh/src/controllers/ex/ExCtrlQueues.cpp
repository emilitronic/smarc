// **********************************************************************
// smesh/src/controllers/ex/ExCtrlQueues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Execute-controller queue implementations.
*/

#include "ExCtrlQueues.hpp"

namespace smesh {

ExCtrlCmdQueue::ExCtrlCmdQueue(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateHeadView).reads(state_Q_).writes(head_val, head_bits);
  UPDATE(updateReady).reads(state_Q_).writes(cmd_rdy);
  UPDATE(updateStorage).reads(state_Q_, cmd_val, cmd_rdy, cmd_bits, pop_count)
                       .writes(state_D_);
}
void ExCtrlCmdQueue::updateReady() {
  cmd_rdy = bit(state_Q_->count < state_Q_->entries.size());
}
// show outside what is at front of queue
void ExCtrlCmdQueue::updateHeadView() {
  const auto state = *state_Q_;
  for (std::size_t i = 0; i < kExCtrlCmdWindow; ++i) {
    head_val[i] = bit(i < state.count);
    head_bits[i] = i < state.count ? state.entries[i] : SmeshIssue{};
  }
}
// 1) remove old commands from front if pop_count asks, 2) accept new commands at back if there's room
void ExCtrlCmdQueue::updateStorage() {
  auto next = *state_Q_;
  const auto current_count = static_cast<std::size_t>(static_cast<unsigned>(next.count));
  const std::size_t requested_pop = static_cast<std::size_t>(static_cast<unsigned>(*pop_count));
  const std::size_t bounded_pop   = requested_pop > 2    ? 2      : requested_pop;
  const std::size_t actual_pop    = bounded_pop > current_count ? current_count : bounded_pop;

  if (actual_pop > 0) {
    const auto popped = next.entries[0];
    for (std::size_t i = actual_pop; i < current_count; ++i) {
      next.entries[i - actual_pop] = next.entries[i];
    }
    for (std::size_t i = current_count - actual_pop; i < current_count; ++i) {
      next.entries[i] = SmeshIssue{};
    }
    next.count = static_cast<std::uint8_t>(current_count - actual_pop);

    trace("ex_cmd_queue: popped count=%u first_tag=%u first_funct=%u",
          static_cast<unsigned>(actual_pop),
          static_cast<unsigned>(popped.rs_tag),
          static_cast<unsigned>(popped.cmd.funct));
  }

  if (cmd_val == 1 && cmd_rdy == 1) {
    const auto issue = *cmd_bits;
    const auto slot = static_cast<std::size_t>(static_cast<unsigned>(next.count));
    next.entries[slot] = issue;
    next.count = static_cast<std::uint8_t>(slot + 1);

    trace("ex_cmd_queue: accepted tag=%u funct=%u",
          static_cast<unsigned>(issue.rs_tag),
          static_cast<unsigned>(issue.cmd.funct));
  }
  state_D_ = next;
}

void ExCtrlCmdQueue::reset() {
  state_D_.reset(QueueState{});
  cmd_rdy.reset(0);

  for (std::size_t i = 0; i < kExCtrlCmdWindow; ++i) {
    head_val[i].reset(0);
    head_bits[i].reset(SmeshIssue{});
  }
}

} // namespace smesh
