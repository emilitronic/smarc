// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulStC.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Includes ordinary and full-width MVOUT cmds, execute-progress gating, and LayerNorm/Softmax
config-and-store sequence.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

// Parameters captured when the C-store generator accepts a loop slot.
struct LoopMatmulStCReq {
  std::uint16_t max_k = 0;
  std::uint16_t max_j = 0;
  std::uint16_t max_i = 0;
  std::uint16_t pad_j = 0;
  std::uint16_t pad_i = 0;
  std::uint64_t dram_addr = 0;
  std::uint64_t dram_stride = 0;
  std::uint32_t addr_start = 0;
  std::uint8_t act = 0;
  std::uint8_t loop_id = 0;
  bit full_c = false;
  bit is_resadd = false;
};

// Emits MVOUT commands, including normalization setup and row stores.
class LoopMatmulStC : public Component {
  DECLARE_COMPONENT(LoopMatmulStC);

 public:
  LoopMatmulStC(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,              req_val);
  Output(bit,             req_rdy);
  Input(LoopMatmulStCReq, req_bits);

  Output(bit,      cmd_val);
  Input(bit,       cmd_rdy);
  Output(SmeshCmd, cmd_bits);

  Input(u16, ex_k);
  Input(u16, ex_j);
  Input(u16, ex_i);
  Input(bit, ex_completed);
  Input(bit, st_utilization_at_limit);

  Output(u16, j);
  Output(u16, i);
  Output(bit, idle);
  Output(u8, loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  enum class Phase : std::uint8_t { Idle, Store, NormConfig, NormStore };

  struct State {
    LoopMatmulStCReq req{};
    u16 j = 0;
    u16 i = 0;
    u16 ln_row = 0;
    u16 ln_cmd = 0;
    u16 ln_stat_id = 0;
    Phase phase = Phase::Idle;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
