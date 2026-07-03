// **********************************************************************
// smesh/src/ExCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "ExCtrl.hpp"

namespace smesh {

ExCtrl::ExCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_in).writes(completed);
}

void ExCtrl::update() {
  if (active_valid_) {
    if (completed.full()) {
      return;
    }
    completed.push(active_rob_id_);
    trace("ex_ctrl: completed rob=%u", static_cast<unsigned>(active_rob_id_));
    active_valid_ = false;
    return;
  }

  if (cmd_in.empty()) {
    return;
  }

  const auto issue = cmd_in.pop();
  active_rob_id_ = issue.rob_id;
  active_valid_ = true;
  trace("ex_ctrl: accepted rob=%u", static_cast<unsigned>(active_rob_id_));
}

void ExCtrl::reset() {
  active_valid_ = false;
  active_rob_id_ = 0;
}

} // namespace smesh
