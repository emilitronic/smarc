// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulExUtilization.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Counts accepted execute commands awaiting RS completion and pauses command
generation when the configured RS entry limit is reached.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshConfig.hpp"

namespace smesh {

class LoopMatmulExUtilization : public Component {
  DECLARE_COMPONENT(LoopMatmulExUtilization);

 public:
  LoopMatmulExUtilization(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, ex_cmd_fire);
  Input(u8, ex_completed);

  Output(u16, outstanding);
  Output(bit, ex_utilization_at_limit);

  void updateStatus();
  void updateNextState();
  void reset();

 private:
  Output(u16, count_Q_);
  Register(u16, count_D_);
};

} // namespace smesh
