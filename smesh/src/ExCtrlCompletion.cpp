// **********************************************************************
// smesh/src/ExCtrlCompletion.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 27 2026

#include "ExCtrlCompletion.hpp"

namespace smesh {

ExCtrlCompletion::ExCtrlCompletion(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateCompletionView)
      .reads(config_val,
             config_rs_tag_valid,
             config_rs_tag,
             mesh_completed_rs_tag_fire,
             mesh_completed_bits)
      .writes(pending_completed_valid, completed_val, completed_bits);
  UPDATE(updatePendingState)
      .reads(config_val,
             config_rs_tag_valid,
             mesh_completed_rs_tag_fire,
             pending_completed_set_val,
             pending_completed_set_bits);
}

void ExCtrlCompletion::updateCompletionView() {
  const bool config_completion = config_val != 0;
  const bool mesh_completion   = mesh_completed_rs_tag_fire != 0;

  pending_completed_valid = bit(pending_completed_valid_[0] ||
                                pending_completed_valid_[1]);
  completed_val  = 0;
  completed_bits = 0;

  if (config_completion) {
    completed_val  = config_rs_tag_valid;
    completed_bits = config_rs_tag;
  } else if (mesh_completion) {
    completed_val  = 1;
    completed_bits = mesh_completed_bits;
  } else if (pending_completed_valid_[0]) {
    completed_val  = 1;
    completed_bits = pending_completed_bits_[0];
  } else if (pending_completed_valid_[1]) {
    completed_val  = 1;
    completed_bits = pending_completed_bits_[1];
  }
}

void ExCtrlCompletion::updatePendingState() {
  const bool config_completion = config_val != 0;
  const bool mesh_completion   = mesh_completed_rs_tag_fire != 0;
  const bool pending_completion = !config_completion && !mesh_completion &&
                                  (pending_completed_valid_[0] ||
                                   pending_completed_valid_[1]);

  if (pending_completion) {
    if (pending_completed_valid_[0]) {
      pending_completed_valid_[0] = false;
      pending_completed_bits_[0] = 0;
    } else {
      pending_completed_valid_[1] = false;
      pending_completed_bits_[1] = 0;
    }
  }

  for (std::size_t i = 0; i < kPendingEntries; ++i) {
    if (pending_completed_set_val[i] != 0) {
      pending_completed_valid_[i] = true;
      pending_completed_bits_[i] = *pending_completed_set_bits[i];
    }
  }

  if ((config_completion && config_rs_tag_valid != 0) ||
      mesh_completion || pending_completion) {
    ++complete_bits_count_;
  }
}

void ExCtrlCompletion::reset() {
  pending_completed_valid_[0] = false;
  pending_completed_valid_[1] = false;
  pending_completed_bits_[0] = 0;
  pending_completed_bits_[1] = 0;
  complete_bits_count_ = 0;

  pending_completed_valid.reset(0);
  completed_val.reset(0);
  completed_bits.reset(0);
}

} // namespace smesh
