// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulUtilization.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 13 2026
/*
Tracks outstanding LoopMatmul load, execute, and store commands and applies
the corresponding reservation-station capacity limits.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshConfig.hpp"

namespace smesh {

class LoopMatmulUtilization : public Component {
  DECLARE_COMPONENT(LoopMatmulUtilization);

 public:
  LoopMatmulUtilization(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, lda_cmd_fire);
  Input(bit, ldb_cmd_fire);
  Input(bit, ldd_cmd_fire);
  Input(u8,  ld_completed);
  Input(bit, ex_cmd_fire);
  Input(u8,  ex_completed);
  Input(bit, st_c_cmd_fire);
  Input(bit, st_c_spad_cmd_fire);
  Input(u8,  st_completed);

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
  Output(u16, ld_count_Q_);
  Register(u16, ld_count_D_);
  Output(u16, ex_count_Q_);
  Register(u16, ex_count_D_);
  Output(u16, st_count_Q_);
  Register(u16, st_count_D_);
};

} // namespace smesh
