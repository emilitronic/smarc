// **********************************************************************
// smesh/include/loop_matmul/LoopMatmul.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026
/*
Connects LoopMatmulState, the primitive command generators, utilization counters,
and command arbiters. The state block owns saved loop setup and worker progress;
this parent builds worker requests and routes generated or ordinary commands.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "LoopMatmulState.hpp"
#include "LoopMatmulLdA.hpp"
#include "LoopMatmulLdB.hpp"
#include "LoopMatmulLdD.hpp"
#include "LoopMatmulEx.hpp"
#include "LoopMatmulStC.hpp"
#include "LoopMatmulStCSpad.hpp"

#include <cstdint>

namespace smesh {

class LoopMatmulCmdQueue;
class LoopMatmulLdABArb;
class LoopMatmulCmdArb;
class LoopMatmulUtilization;

// Captures LOOP_WS setup and routes generated or ordinary commands to the output.
class LoopMatmul : public Component {
  DECLARE_COMPONENT(LoopMatmul);

 public:
  LoopMatmul(std::string name, COMPONENT_CTOR);
  ~LoopMatmul() override;

  Clock(clk);

  Input(bit,            in_val);
  Input(SmeshQueuedCmd, in_bits);
  Output(bit,           in_rdy);

  Output(bit,            out_val);
  Output(SmeshQueuedCmd, out_bits);
  Input(bit,             out_rdy);

  Output(bit,            busy);
  Input(u8,              ld_completed);
  Input(u8,              st_completed);
  Input(u8,              ex_completed);
  // One-cycle pulses when the corresponding loop slot is released.
  Output(bit,            completed0);
  Output(bit,            completed1);
  Output(u8,             head_loop_id);
  Output(LoopMatmulSlot, loop0);
  Output(LoopMatmulSlot, loop1);

  void updateDecision();
  void updateStatus();
  void updateGeneratorInputs();
  void updateProgress();
  void updateCommandFires();
  void reset();

 private:
  LoopMatmulState*    loop_state_ = nullptr;
  LoopMatmulCmdQueue* cmd_queue_ = nullptr;
  LoopMatmulLdA*      ld_a_      = nullptr;
  LoopMatmulLdB*      ld_b_      = nullptr;
  LoopMatmulLdD*      ld_d_      = nullptr;
  LoopMatmulEx*       ex_        = nullptr;
  LoopMatmulStC*      st_c_      = nullptr;
  LoopMatmulStCSpad*  st_c_spad_ = nullptr;
  LoopMatmulLdABArb*  ld_ab_arb_ = nullptr;
  LoopMatmulCmdArb*   cmd_arb_   = nullptr;
  LoopMatmulUtilization* utilization_ = nullptr;

  Input(bit, head_val_);
  Input(SmeshQueuedCmd, head_bits_);
  Output(bit, head_rdy_);

  Input(bit, unrolled_val_);
  Input(SmeshCmd, unrolled_bits_);
  Output(bit, is_resadd_);

  Output(bit, ld_a_req_val_);
  Input(bit, ld_a_req_rdy_);
  Output(LoopMatmulLdAReq, ld_a_req_bits_);
  Output(bit, ld_b_req_val_);
  Input(bit, ld_b_req_rdy_);
  Output(LoopMatmulLdBReq, ld_b_req_bits_);
  Output(bit, ld_d_req_val_);
  Input(bit, ld_d_req_rdy_);
  Output(LoopMatmulLdDReq, ld_d_req_bits_);
  Output(bit, ex_req_val_);
  Input(bit, ex_req_rdy_);
  Output(LoopMatmulExReq, ex_req_bits_);
  Output(bit, st_c_req_val_);
  Input(bit, st_c_req_rdy_);
  Output(LoopMatmulStCReq, st_c_req_bits_);
  Output(bit, st_c_spad_req_val_);
  Input(bit, st_c_spad_req_rdy_);
  Output(LoopMatmulStCSpadReq, st_c_spad_req_bits_);

  Input(bit, ld_a_idle_);
  Input(u8, ld_a_loop_id_);
  Input(bit, ld_b_idle_);
  Input(u8, ld_b_loop_id_);
  Input(bit, ld_d_idle_);
  Input(u8, ld_d_loop_id_);
  Input(bit, ex_idle_);
  Input(u8, ex_loop_id_);
  Input(bit, st_c_idle_);
  Input(u8, st_c_loop_id_);
  Input(bit, st_c_spad_idle_);
  Input(u8, st_c_spad_loop_id_);

  Output(bit, ld_a_cmd_fire_);
  Output(bit, ld_b_cmd_fire_);
  Output(bit, ld_d_cmd_fire_);
  Output(bit, ex_cmd_fire_);
  Output(bit, st_c_cmd_fire_);
  Output(bit, st_c_spad_cmd_fire_);
  Input(bit, ld_a_cmd_val_);
  Input(bit, ld_a_cmd_rdy_);
  Input(bit, ld_b_cmd_val_);
  Input(bit, ld_b_cmd_rdy_);
  Input(bit, ld_d_cmd_val_);
  Input(bit, ld_d_cmd_rdy_);
  Input(bit, ex_cmd_val_);
  Input(bit, ex_cmd_rdy_);
  Input(bit, st_c_cmd_val_);
  Input(bit, st_c_cmd_rdy_);
  Input(bit, st_c_spad_cmd_val_);
  Input(bit, st_c_spad_cmd_rdy_);
  Output(bit, lda_complete_to_ex_);
  Output(bit, ldb_complete_to_ex_);
  Output(bit, ldd_complete_to_ex_);
  Output(bit, ex_complete_to_st_c_);
  Output(bit, ex_complete_to_st_c_spad_);

  Input(LoopMatmulStateData, state_view_);
};

} // namespace smesh
