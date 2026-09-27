// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulLdUtilization.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Counts accepted commands from LdA/LdB/LdB, subtracts the RS load-completion count,
and assert ld_utiliation_at_limit at the configured load-entry limit.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshConfig.hpp"

namespace smesh {

// Tracks issued LoopMatmul load commands awaiting RS completion.
class LoopMatmulLdUtilization : public Component {
  DECLARE_COMPONENT(LoopMatmulLdUtilization);

 public:
  LoopMatmulLdUtilization(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, lda_cmd_fire);
  Input(bit, ldb_cmd_fire);
  Input(bit, ldd_cmd_fire);
  Input(u8,  ld_completed);

  Output(u16, outstanding);
  Output(bit, ld_utilization_at_limit);

  void updateStatus();
  void updateNextState();
  void reset();

 private:
  Output(u16, count_Q_);
  Register(u16, count_D_);
};

} // namespace smesh
