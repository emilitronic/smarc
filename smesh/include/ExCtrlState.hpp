// **********************************************************************
// smesh/include/ExCtrlState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 5 2026

#pragma once

#include <cascade/Cascade.hpp>

#include <cstdint>

namespace smesh {

enum class ExCtrlFsmState : std::uint8_t {
  WaitingForCmd = 0,
  Compute       = 1,
  Flush         = 2,
  Flushing      = 3,
};

inline ExCtrlFsmState toFsmState(u8 value) {
  return static_cast<ExCtrlFsmState>(static_cast<std::uint8_t>(value));
}

class ExCtrlState : public Component {
  DECLARE_COMPONENT(ExCtrlState);

 public:
  ExCtrlState(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Output(u8, control_state); // current FSM state

  void update();
  void reset();

 private:
  Register(u8, control_state_reg_); // next FSM state
};

} // namespace smesh
