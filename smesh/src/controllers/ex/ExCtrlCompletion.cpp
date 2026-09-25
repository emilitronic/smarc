// **********************************************************************
// smesh/src/controllers/ex/ExCtrlCompletion.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 27 2026

#include "ExCtrlCompletion.hpp"

namespace smesh {

TraceKey(ex_ctrl_completion_view);

ExCtrlCompletion::ExCtrlCompletion(std::string /*name*/, IMPL_CTOR) {
  // update Q from D on clock edge before executing update() for this cycle
  pending_completed_Q_ <= pending_completed_D_;

  UPDATE(updatePendingView)
      .reads(pending_completed_Q_)
      .writes(pending_completed_val);
  UPDATE(updateCompletionView)
      .reads(config_val, config_rs_tag_val, config_rs_tag,
             mesh_completed_rs_tag_fire, mesh_completed_bits,
             pending_completed_Q_)
      .writes(completed_val, completed_bits);
  UPDATE(updatePendingState)
      .reads(config_val, config_rs_tag_val,
             mesh_completed_rs_tag_fire,
             pending_completed_set_val, pending_completed_set_bits, pending_completed_Q_)
      .writes(pending_completed_D_);
}

void ExCtrlCompletion::updatePendingView() {
  const auto pending = *pending_completed_Q_; // commited pending state visible this cycle
  pending_completed_val = bit(pending.val[0] == 1 || pending.val[1] == 1);
}

void ExCtrlCompletion::updateCompletionView() {
  const auto pending = *pending_completed_Q_; // commited pending state visible this cycle
  const bool config_completion = config_val == 1;
  const bool mesh_completion   = mesh_completed_rs_tag_fire == 1;

  completed_val  = 0;
  completed_bits = 0;

  if (config_completion) {
    completed_val  = config_rs_tag_val;
    completed_bits = *config_rs_tag;
  } else if (mesh_completion) {
    completed_val  = 1;
    completed_bits = *mesh_completed_bits;
  } else if (pending.val[0] == 1) {
    completed_val  = 1;
    completed_bits = pending.bits[0];
  } else if (pending.val[1] == 1) {
    completed_val  = 1;
    completed_bits = pending.bits[1];
  }
}

void ExCtrlCompletion::updatePendingState() {
  const auto pending = *pending_completed_Q_; // commited pending state visible this cycle
  auto next = pending;

  trace(ex_ctrl_completion_view,
        "set_val0=%u set_val1=%u pending0=%u pending1=%u\n",
        static_cast<unsigned>(pending_completed_set_val[0]),
        static_cast<unsigned>(pending_completed_set_val[1]),
        static_cast<unsigned>(pending.val[0]),
        static_cast<unsigned>(pending.val[1]));

  const bool config_completion  = config_val == 1;
  const bool mesh_completion    = mesh_completed_rs_tag_fire == 1;
  const bool pending_completion = !config_completion && !mesh_completion &&
                                  (pending.val[0] == 1 || pending.val[1] == 1);

  if (pending_completion) {
    if (pending.val[0] == 1) {
      next.val[0]  = 0;
      next.bits[0] = 0;
    } else {
      next.val[1]  = 0;
      next.bits[1] = 0;
    }
  }

  for (std::size_t i = 0; i < kPendingEntries; ++i) {
    if (pending_completed_set_val[i] == 1) {
      next.val[i]  = 1;
      next.bits[i] = *pending_completed_set_bits[i];
    }
  }

  pending_completed_D_ = next; // pending state committed on next clock edge
}

void ExCtrlCompletion::reset() {
  pending_completed_D_.reset(ExCtrlPendingCompletionState{});

  pending_completed_val.reset(0);
  completed_val.reset(0);
  completed_bits.reset(0);
}

} // namespace smesh
