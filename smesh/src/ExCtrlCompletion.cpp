// **********************************************************************
// smesh/src/ExCtrlCompletion.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 27 2026

#include "ExCtrlCompletion.hpp"

namespace smesh {

ExCtrlCompletion::ExCtrlCompletion(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updatePendingView)
      .writes(pending_completed_valid);
  UPDATE(updateConfigCompletion)
      .reads(config_val, config_rs_tag_valid, config_rs_tag)
      .writes(completed);
}
// are any pending completion registers occupied?
void ExCtrlCompletion::updatePendingView() {
  pending_completed_valid = bit(pending_completed_valid_[0] || pending_completed_valid_[1]);
}
// direct the correct signal to completion block completed output
void ExCtrlCompletion::updateConfigCompletion() {
  if (config_val != 0 && config_rs_tag_valid != 0 && !completed.full()) {
    completed.push(*config_rs_tag);
  }
}

void ExCtrlCompletion::reset() {
  pending_completed_valid_[0] = false;
  pending_completed_valid_[1] = false;
  pending_completed_bits_[0] = 0;
  pending_completed_bits_[1] = 0;

  pending_completed_valid.reset(0);
}

} // namespace smesh
