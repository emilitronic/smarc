// **********************************************************************
// smesh/src/ExCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 5 2026

#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlState::ExCtrlState(std::string /*name*/, IMPL_CTOR) {
  control_state <= control_state_reg_;
}

void ExCtrlState::update() {
  const auto state = toFsmState(*control_state);
   
  switch (state) {
    case ExCtrlFsmState::WaitingForCmd: {
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
