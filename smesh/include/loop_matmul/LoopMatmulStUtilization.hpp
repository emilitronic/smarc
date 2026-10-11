// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulStUtilization.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Counts accepted store commands awaiting RS completion and pauses command
generation when the configured RS entry limit is reached.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshConfig.hpp"

namespace smesh {

class LoopMatmulStUtilization : public Component {
  DECLARE_COMPONENT(LoopMatmulStUtilization);

 public:
  LoopMatmulStUtilization(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, st_c_cmd_fire);
  Input(bit, st_c_spad_cmd_fire);
  Input(u8, st_completed);

  Output(u16, outstanding);
  Output(bit, st_utilization_at_limit);

  void updateStatus();
  void updateNextState();
  void reset();

 private:
  Output(u16, count_Q_);
  Register(u16, count_D_);
};

} // namespace smesh
