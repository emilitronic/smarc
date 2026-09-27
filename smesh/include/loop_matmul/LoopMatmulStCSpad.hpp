// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulStCSpad.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Emits one STORE_SPAD command per C tiles, with accumulator source addreses,
scratchpad destinations, edge padding, and execute-progress gating.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

// Parameters captured when a loop slot starts storing C into scratchpad.
struct LoopMatmulStCSpadReq {
  std::uint16_t max_k = 0;
  std::uint16_t max_j = 0;
  std::uint16_t max_i = 0;
  std::uint16_t pad_j = 0;
  std::uint16_t pad_i = 0;
  std::uint32_t dst_addr = 0;
  std::uint32_t src_addr = 0;
  bit full_c = false;
  std::uint8_t act = 0;
  std::uint8_t loop_id = 0;
  bit is_resadd = false;
};

// Emits one STORE_SPAD command per C tile after execute has advanced past it.
class LoopMatmulStCSpad : public Component {
  DECLARE_COMPONENT(LoopMatmulStCSpad);

 public:
  LoopMatmulStCSpad(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit,                  req_val);
  Output(bit,                 req_rdy);
  Input(LoopMatmulStCSpadReq, req_bits);

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
  Output(u8,  loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  struct State {
    LoopMatmulStCSpadReq req{};
    u16 j = 0;
    u16 i = 0;
    bit active = false;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
