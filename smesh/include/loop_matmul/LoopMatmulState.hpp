// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 13 2026
/*
Owns two saved LOOP_WS configurations, their worker progress, head selection,
and accumulator-region offsets. Accepted setup commands update their fields;
worker request acceptance marks starts and worker idle marks generation complete.
After all workers finish, the head slot becomes available for another loop.
RS execution completion is accounted for separately by LoopMatmulUtilization.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstdint>

namespace smesh {

// One pending LOOP_WS operation, including the fields collected from its setup commands.
struct LoopMatmulSlot {
  std::uint16_t max_i = 0;
  std::uint16_t max_j = 0;
  std::uint16_t max_k = 0;
  std::uint16_t pad_i = 0;
  std::uint16_t pad_j = 0;
  std::uint16_t pad_k = 0;

  std::uint64_t a_dram_addr = 0;
  std::uint64_t b_dram_addr = 0;
  std::uint64_t d_dram_addr = 0;
  std::uint64_t c_dram_addr = 0;
  std::uint64_t a_dram_stride = 0;
  std::uint64_t b_dram_stride = 0;
  std::uint64_t d_dram_stride = 0;
  std::uint64_t c_dram_stride = 0;

  std::uint32_t a_addr_start = 0;
  std::uint32_t b_addr_end = 0;
  std::uint32_t resadd_addr_start = 0;
  std::uint32_t c_spad_addr = 0;
  std::uint8_t  a_ex_spad_id = 0;
  std::uint8_t  b_ex_spad_id = 0;
  std::uint8_t  act = 0;
  bit a_transpose   = false;
  bit b_transpose   = false;
  bit ex_accumulate = false;
  bit full_c        = false;
  bit low_d         = false;
  bit inc_acc_addr  = false;
  bit spad_only     = false;
  bit configured   = false;
  bit running      = false;

  bit lda_started   = false;
  bit ldb_started   = false;
  bit ldd_started   = false;
  bit ex_started    = false;
  bit st_started    = false;
  bit lda_completed = false;
  bit ldb_completed = false;
  bit ldd_completed = false;
  bit ex_completed  = false;
  bit st_completed  = false;
};

// Current registered state exposed to the parent's worker-request logic.
struct LoopMatmulStateData {
  std::array<LoopMatmulSlot, 2> loops{};
  u8  head_id = 0;
  bit is_resadd = false;
  u32 ld_d_addr_start = 0;
  u32 ex_c_addr_start = 0;
  u32 st_c_addr_start = 0;
};

class LoopMatmulState : public Component {
  DECLARE_COMPONENT(LoopMatmulState);

 public:
  LoopMatmulState(std::string name, COMPONENT_CTOR);
  Clock(clk);

  // The parent supplies ready; only accepted LOOP_WS setup/launch commands modify state.
  Input(bit, head_val);
  Input(SmeshQueuedCmd, head_bits);
  Input(bit, head_rdy);

  Input(bit, ld_a_req_val);
  Input(bit, ld_a_req_rdy);
  Input(bit, ld_b_req_val);
  Input(bit, ld_b_req_rdy);
  Input(bit, ld_d_req_val);
  Input(bit, ld_d_req_rdy);
  Input(bit, ex_req_val);
  Input(bit, ex_req_rdy);
  Input(bit, st_c_req_val);
  Input(bit, st_c_req_rdy);
  Input(bit, st_c_spad_req_val);
  Input(bit, st_c_spad_req_rdy);

  Input(bit, ld_a_idle);
  Input(u8, ld_a_loop_id);
  Input(bit, ld_b_idle);
  Input(u8, ld_b_loop_id);
  Input(bit, ld_d_idle);
  Input(u8, ld_d_loop_id);
  Input(bit, ex_idle);
  Input(u8, ex_loop_id);
  Input(bit, st_c_idle);
  Input(u8, st_c_loop_id);
  Input(bit, st_c_spad_idle);
  Input(u8, st_c_spad_loop_id);

  Output(LoopMatmulStateData, state);
  Output(LoopMatmulSlot, loop0);
  Output(LoopMatmulSlot, loop1);
  Output(u8, head_loop_id);
  // Pulses when a finished command stream releases its corresponding slot.
  Output(bit, completed0);
  Output(bit, completed1);

  void updateView();
  void updateNextState();
  void reset();

 private:
  Output(LoopMatmulStateData, state_Q_);
  Register(LoopMatmulStateData, state_D_);
};

} // namespace smesh
