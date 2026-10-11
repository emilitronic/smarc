// **********************************************************************
// smesh/include/loop_conv/LoopConvScheduler.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Builds and offers the load-bias worker request from the saved loop slots.
Other worker request paths will be added one at a time.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "LoopConvState.hpp"
#include "LoopConvLdBias.hpp"

namespace smesh {

class LoopConvScheduler : public Component {
  DECLARE_COMPONENT(LoopConvScheduler);

 public:
  LoopConvScheduler(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Input(LoopConvSlot, loop0);
  Input(LoopConvSlot, loop1);
  Input(u8,           head_loop_id);

  Output(bit,               ld_bias_req_val);
  Output(LoopConvLdBiasReq, ld_bias_req_bits);
  Input(bit,                ld_bias_req_rdy);
  // Acceptance events tell the state owner which slot's worker has started.
  Output(bit, ld_bias_req_fire);
  Output(u8,  ld_bias_loop_id);

  void updateRequest();
  void updateTransfer();
  void updateAddress();
  void reset();

 private:
  Output(u32,   ld_bias_addr_start_Q_);
  Register(u32, ld_bias_addr_start_D_);
};

} // namespace smesh
