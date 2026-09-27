// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulEx.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Emits PRELOAD/COMPUTE command pairs for one LOOP_WS operation, using the current
loop indices and load-completion status to determine when to issue each command.
Waits for A/B/D/ load progress, handles execute backpressure, and advances
through k, j, and i.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

// TODO: Original's B-load progress check uses ld_ka (not ld_kb) in its equal-k case.
// Confirm whether that is intentional before changing this condition.

// Parameters captured when the execute generator accepts a loop slot.
struct LoopMatmulExReq {
  std::uint16_t max_j = 0;
  std::uint16_t max_k = 0;
  std::uint16_t max_i = 0;
  std::uint16_t pad_j = 0;
  std::uint16_t pad_k = 0;
  std::uint16_t pad_i = 0;
  bit           a_transpose  = false;
  bit           b_transpose  = false;
  bit           accumulate   = false;
  std::uint32_t a_addr_start = 0;
  std::uint32_t b_addr_end   = 0;
  std::uint32_t c_addr_start = 0;
  std::uint8_t loop_id       = 0;
  bit           skip         = false;
};

// Emits PRELOAD/COMPUTE command pairs for one LOOP_WS operation.
class LoopMatmulEx : public Component {
  DECLARE_COMPONENT(LoopMatmulEx);

 public:
  LoopMatmulEx(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,             req_val);
  Output(bit,            req_rdy);
  Input(LoopMatmulExReq, req_bits);

  Output(bit,            cmd_val);
  Input(bit,             cmd_rdy);
  Output(SmeshCmd,       cmd_bits);

  Input(u16,             ld_ka);
  Input(u16,             ld_kb);
  Input(u16,             ld_j);
  Input(u16,             ld_i);
  Input(bit,             lda_completed);
  Input(bit,             ldb_completed);
  Input(bit,             ldd_completed);
  Input(bit,             ex_utilization_at_limit);

  Output(u16,            k);
  Output(u16,            j);
  Output(u16,            i);
  Output(bit,            idle);
  Output(u8,             loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  enum class Phase : std::uint8_t { Idle, Preload, Compute };

  struct State {
    LoopMatmulExReq req{};
    u16 k = 0;
    u16 j = 0;
    u16 i = 0;
    Phase phase = Phase::Idle;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
