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
  UPDATE(updateHeadView).writes(head_val, head_bits);
  UPDATE(updateStorage).reads(cmd_in, head_val, head_rdy);
}

// Present the command that the decoder and FSM should inspect this cycle.
void StCtrlCmdQueue::updateHeadView() {
  head_val = bit(count_ != 0);
  head_bits = count_ != 0 ? entries_[0] : SmeshIssue{};
}

// Pop the accepted command, then fill any available queue slot from cmd_in.
void StCtrlCmdQueue::updateStorage() {
  const bool head_fire = head_val != 0 && head_rdy != 0;
  if (head_fire) {
    const auto popped = entries_[0];
    for (std::size_t i = 1; i < count_; ++i) {
      entries_[i - 1] = entries_[i];
    }
    entries_[count_ - 1] = SmeshIssue{};
    --count_;

    trace("st_cmd_queue: popped tag=%u funct=%u",
          static_cast<unsigned>(popped.rs_tag),
          static_cast<unsigned>(popped.cmd.funct));
  }

  if (cmd_in.empty() || count_ >= entries_.size()) {
    return;
  }

  const auto issue = cmd_in.pop();
  entries_[count_] = issue;
  ++count_;

  trace("st_cmd_queue: accepted tag=%u funct=%u",
        static_cast<unsigned>(issue.rs_tag),
        static_cast<unsigned>(issue.cmd.funct));
}

void StCtrlCmdQueue::reset() {
  entries_ = {};
  count_ = 0;
  head_val.reset(0);
  head_bits.reset(SmeshIssue{});
}

} // namespace smesh
