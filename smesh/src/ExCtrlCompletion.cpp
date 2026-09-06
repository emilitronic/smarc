// **********************************************************************
// smesh/src/ExCtrlCompletion.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 27 2026

#include "ExCtrlCompletion.hpp"

namespace smesh {

TraceKey(ex_ctrl_completion_view);

ExCtrlCompletion::ExCtrlCompletion(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updatePendingView)
      .writes(pending_completed_val);
  UPDATE(updateCompletionView)
      .reads(config_val,
             config_rs_tag_val,
             config_rs_tag,
             mesh_completed_rs_tag_fire,
             mesh_completed_bits)
      .writes(completed_val, completed_bits);
  UPDATE(updatePendingState)
      .reads(config_val,
             config_rs_tag_val,
             mesh_completed_rs_tag_fire,
             pending_completed_set_val,
             pending_completed_set_bits);
}

void ExCtrlCompletion::updatePendingView() {
  pending_completed_val = bit(pending_completed_val_[0] ||
                              pending_completed_val_[1]);
}

void ExCtrlCompletion::updateCompletionView() {
  const bool config_completion = config_val != 0;
  const bool mesh_completion   = mesh_completed_rs_tag_fire != 0;

  completed_val  = 0;
  completed_bits = 0;

  if (config_completion) {
    completed_val  = config_rs_tag_val;
    completed_bits = *config_rs_tag;
  } else if (mesh_completion) {
    completed_val  = 1;
    completed_bits = *mesh_completed_bits;
  } else if (pending_completed_val_[0]) {
    completed_val  = 1;
    completed_bits = pending_completed_bits_[0];
  } else if (pending_completed_val_[1]) {
    completed_val  = 1;
    completed_bits = pending_completed_bits_[1];
  }
}

void ExCtrlCompletion::updatePendingState() {
  trace(ex_ctrl_completion_view,
        "set_val0=%u set_val1=%u pending0=%u pending1=%u\n",
        static_cast<unsigned>(pending_completed_set_val[0]),
        static_cast<unsigned>(pending_completed_set_val[1]),
        static_cast<unsigned>(pending_completed_val_[0]),
        static_cast<unsigned>(pending_completed_val_[1]));

  const bool config_completion = config_val != 0;
  const bool mesh_completion   = mesh_completed_rs_tag_fire != 0;
  const bool pending_completion = !config_completion && !mesh_completion &&
                                  (pending_completed_val_[0] ||
                                   pending_completed_val_[1]);

  if (pending_completion) {
    if (pending_completed_val_[0]) {
      pending_completed_val_[0] = false;
      pending_completed_bits_[0] = 0;
    } else {
      pending_completed_val_[1] = false;
      pending_completed_bits_[1] = 0;
    }
  }

  for (std::size_t i = 0; i < kPendingEntries; ++i) {
    if (pending_completed_set_val[i] != 0) {
      pending_completed_val_[i] = true;
      pending_completed_bits_[i] = *pending_completed_set_bits[i];
    }
  }

  if ((config_completion && config_rs_tag_val != 0) ||
      mesh_completion || pending_completion) {
    ++complete_bits_count_;
  }
}

void ExCtrlCompletion::reset() {
  pending_completed_val_[0] = false;
  pending_completed_val_[1] = false;
  pending_completed_bits_[0] = 0;
  pending_completed_bits_[1] = 0;
  complete_bits_count_ = 0;

  pending_completed_val.reset(0);
  completed_val.reset(0);
  completed_bits.reset(0);
}

} // namespace smesh
