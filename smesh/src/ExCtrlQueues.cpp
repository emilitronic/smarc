// **********************************************************************
// smesh/src/ExCtrlQueues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Execute-controller queue implementations.
*/

#include "ExCtrlQueues.hpp"

namespace smesh {

ExCtrlCmdQueue::ExCtrlCmdQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateHeadView).writes(head_val, head_bits);
  UPDATE(updateStorage).reads(cmd_in, pop_count);
}
// show outside what is at front of queue
void ExCtrlCmdQueue::updateHeadView() {
  for (std::size_t i = 0; i < kExCtrlCmdWindow; ++i) {
    head_val[i] = bit(i < count_);
    head_bits[i] = i < count_ ? entries_[i] : SmeshIssue{};
  }
}
// 1) remove old commands from front if pop_count asks, 2) accept new commands at back if there's room
void ExCtrlCmdQueue::updateStorage() {
  const std::size_t requested_pop = static_cast<std::size_t>(static_cast<unsigned>(*pop_count));
  const std::size_t bounded_pop = requested_pop > 2 ? 2 : requested_pop;
  const std::size_t actual_pop = bounded_pop > count_ ? count_ : bounded_pop;

  if (actual_pop > 0) {
    const auto popped = entries_[0];
    for (std::size_t i = actual_pop; i < count_; ++i) {
      entries_[i - actual_pop] = entries_[i];
    }
    for (std::size_t i = count_ - actual_pop; i < count_; ++i) {
      entries_[i] = SmeshIssue{};
    }
    count_ -= actual_pop;

    trace("ex_cmd_queue: popped count=%u first_tag=%u first_funct=%u",
          static_cast<unsigned>(actual_pop),
          static_cast<unsigned>(popped.rs_tag),
          static_cast<unsigned>(popped.cmd.funct));
  }

  if (cmd_in.empty() || count_ >= kExCtrlCmdWindow) {
    return;
  }

  const auto issue = cmd_in.pop();
  entries_[count_] = issue;
  ++count_;

  trace("ex_cmd_queue: accepted tag=%u funct=%u",
        static_cast<unsigned>(issue.rs_tag),
        static_cast<unsigned>(issue.cmd.funct));
}

void ExCtrlCmdQueue::reset() {
  entries_ = {};
  count_ = 0;

  for (std::size_t i = 0; i < kExCtrlCmdWindow; ++i) {
    head_val[i].reset(0);
    head_bits[i].reset(SmeshIssue{});
  }
}

} // namespace smesh
