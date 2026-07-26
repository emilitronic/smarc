// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(state);
}

void ExCtrlState::update() {
  state = static_cast<std::uint8_t>(state_);
}

void ExCtrlState::reset() {
  state_ = ExCtrlFsmState::WaitingForCmd;
  state.reset(static_cast<std::uint8_t>(state_));
}

} // namespace smesh
