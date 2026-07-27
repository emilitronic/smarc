// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(head_val, head_bits, do_config, do_preloads, do_computes);
}

void ExCtrlState::update() {}

void ExCtrlState::reset() {
  state_ = ExCtrlFsmState::WaitingForCmd;
}

} // namespace smesh
