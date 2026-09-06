// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 5 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  control_state <= control_state_reg_;

  UPDATE(update)
    .reads(head_val, head_bits)
    .reads(do_config, do_preloads, do_computes)
    .reads(matmul_in_progress, pending_completed_valid);
}

void ExCtrlState::update() {
  const auto state = toFsmState(*control_state);
   
  switch (state) {
    case ExCtrlFsmState::WaitingForCmd: {
      if (head_val[0] == 1 && do_config == 1 && matmul_in_progress == 0 && pending_completed_valid == 0) {
        const auto cmdq  = *head_bits[0];
        const auto rs1   = rawRs1(cmdq);
        const auto rs2   = rawRs2(cmdq);
        const auto kind  = configKind(rs1);
      }
      break;
    }
    case ExCtrlFsmState::Compute: {
      break;
    }
    case ExCtrlFsmState::Flush: {
      break;
    }
    case ExCtrlFsmState::Flushing: {
      break;
    }
  }
}

void ExCtrlState::reset() {
  control_state_reg_.reset(static_cast<std::uint8_t>(ExCtrlFsmState::WaitingForCmd));
}

} // namespace smesh
