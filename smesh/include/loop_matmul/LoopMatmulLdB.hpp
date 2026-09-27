// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulLdB.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Never applies B-row padding because its row comparison is always false. 
This implementation preserves that literal behavior for now.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

// TODO: Original's LdB compares max_row_iterator with itself minus one, so it
// never applies B-row padding. Looks like a bug.  
// Decide whether to preserve or correct that here.

// Parameters captured when the B-load generator accepts a loop slot.
struct LoopMatmulLdBReq {
  std::uint16_t max_k = 0;
  std::uint16_t max_j = 0;
  std::uint16_t pad_k = 0;
  std::uint16_t pad_j = 0;
  std::uint64_t dram_addr = 0;
  std::uint64_t dram_stride = 0;
  std::uint32_t addr_end = 0;
  std::uint8_t loop_id = 0;
  bit transpose = false;
  bit is_resadd = false;
};

// Emits MVIN2 commands for the B tiles of one LOOP_WS operation.
class LoopMatmulLdB : public Component {
  DECLARE_COMPONENT(LoopMatmulLdB);

 public:
  LoopMatmulLdB(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,              req_val);
  Output(bit,             req_rdy);
  Input(LoopMatmulLdBReq, req_bits);

  Output(bit,      cmd_val);
  Input(bit,       cmd_rdy);
  Output(SmeshCmd, cmd_bits);
  Input(bit,       ld_utilization_at_limit);

  Output(u16, k);
  Output(u16, j);
  Output(bit, idle);
  Output(u8,  loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  struct State {
    LoopMatmulLdBReq req{};
    u16 k = 0;
    u16 j = 0;
    bit active = false;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
