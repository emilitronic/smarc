// **********************************************************************
// smesh/include/loop_conv/LoopConvUtilization.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 12 2026
/*
Counts worker commands accepted downstream but not yet completed by the RS.
All three load workers share one count. Execute and Store have separate counts.
CONFIG commands count too; pass-through commands do not.
*/
#pragma once

#include <cascade/Cascade.hpp>
#include "SmeshConfig.hpp"

namespace smesh {

class LoopConvUtilization : public Component {
  DECLARE_COMPONENT(LoopConvUtilization);
 public:
  LoopConvUtilization(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit,  ld_cmd_fire);
  Input(bit,  ex_cmd_fire);
  Input(bit,  st_cmd_fire);
  // RS reports only completions of commands originating from LoopConv.
  Input(u8,   ld_completed);
  Input(u8,   ex_completed);
  Input(u8,   st_completed);
  Output(u16, ld_outstanding);
  Output(u16, ex_outstanding);
  Output(u16, st_outstanding);
  Output(bit, ld_utilization_at_limit);
  Output(bit, ex_utilization_at_limit);
  Output(bit, st_utilization_at_limit);
  void updateStatus();
  void updateNextState();
  void reset();
 private:
  Output(u16,   ld_count_Q_);
  Output(u16,   ex_count_Q_);
  Output(u16,   st_count_Q_);
  Register(u16, ld_count_D_);
  Register(u16, ex_count_D_);
  Register(u16, st_count_D_);
};

} // namespace smesh
