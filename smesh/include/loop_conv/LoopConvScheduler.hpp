// **********************************************************************
// smesh/include/loop_conv/LoopConvScheduler.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Builds and offers load, Execute, and Store requests from saved loop slots.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "LoopConvState.hpp"
#include "LoopConvLdBias.hpp"
#include "LoopConvLdInput.hpp"
#include "LoopConvLdWeight.hpp"
#include "LoopConvExecute.hpp"
#include "LoopConvSt.hpp"

namespace smesh {

class LoopConvScheduler : public Component {
  DECLARE_COMPONENT(LoopConvScheduler);

 public:
  LoopConvScheduler(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Input(LoopConvSlot, loop0);
  Input(LoopConvSlot, loop1);
  Input(u8,           head_loop_id);

  Output(bit,                 ld_bias_req_val);
  Output(LoopConvLdBiasReq,   ld_bias_req_bits);
  Input(bit,                  ld_bias_req_rdy);
  // Acceptance events tell the state owner which slot's worker has started.
  Output(bit,                 ld_bias_req_fire);
  Output(u8,                  ld_bias_loop_id);
  Output(bit,                 ld_input_req_val);
  Output(LoopConvLdInputReq,  ld_input_req_bits);
  Input(bit,                  ld_input_req_rdy);
  Output(bit,                 ld_input_req_fire);
  Output(u8,                  ld_input_loop_id);
  Output(bit,                 ld_weights_req_val);
  Output(LoopConvLdWeightReq, ld_weights_req_bits);
  Input(bit,                  ld_weights_req_rdy);
  Output(bit,                 ld_weights_req_fire);
  Output(u8,                  ld_weights_loop_id);
  Output(bit,                 ex_req_val);
  Output(LoopConvExecuteReq,  ex_req_bits);
  Input(bit,                  ex_req_rdy);
  Output(bit,                 ex_req_fire);
  Output(u8,                  ex_loop_id);
  Output(bit,                 st_req_val);
  Output(LoopConvStReq,       st_req_bits);
  Input(bit,                  st_req_rdy);
  Output(bit,                 st_req_fire);
  Output(u8,                  st_loop_id);

  void updateRequest();
  void updateTransfer();
  void updateAddress();
  void updateInputRequest();
  void updateInputTransfer();
  void updateWeightRequest();
  void updateWeightTransfer();
  void updateExecuteRequest();
  void updateExecuteTransfer();
  void updateExecuteAddress();
  void updateStoreRequest();
  void updateStoreTransfer();
  void updateStoreAddress();
  void reset();

 private:
  Output(u32,   ld_bias_addr_start_Q_);
  Register(u32, ld_bias_addr_start_D_);
  Output(u32,   ex_c_addr_start_Q_);
  Register(u32, ex_c_addr_start_D_);
  Output(u32,   st_addr_start_Q_);
  Register(u32, st_addr_start_D_);
};

} // namespace smesh
