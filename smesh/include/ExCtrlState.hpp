// **********************************************************************
// smesh/include/ExCtrlState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Central execute-controller FSM state holder.
*/

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

class ExCtrlState : public Component {
  DECLARE_COMPONENT(ExCtrlState);

 public:
  ExCtrlState(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Output(u8, state);

  void update();
  void reset();

 private:
  ExCtrlFsmState state_ = ExCtrlFsmState::WaitingForCmd;
};

} // namespace smesh
