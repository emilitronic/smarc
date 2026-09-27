// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulLdA.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Accepts an A-load request and emits MVIN commands, with transpose, 
padding, accumulator destination, DMA block splitting, and 
backpressure handled.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

// Parameters captured when the A-load generator accepts a loop slot.
struct LoopMatmulLdAReq {
  std::uint16_t max_i = 0;
  std::uint16_t max_k = 0;
  std::uint16_t pad_i = 0;
  std::uint16_t pad_k = 0;
  std::uint64_t dram_addr = 0;
  std::uint64_t dram_stride = 0;
  std::uint32_t addr_start = 0;
  std::uint8_t loop_id = 0;
  bit transpose = false;
  bit is_resadd = false;
};

// Emits MVIN commands for the A tiles of one LOOP_WS operation.
class LoopMatmulLdA : public Component {
  DECLARE_COMPONENT(LoopMatmulLdA);

 public:
  LoopMatmulLdA(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,              req_val);
  Output(bit,             req_rdy);
  Input(LoopMatmulLdAReq, req_bits);

  Output(bit,      cmd_val);
  Input(bit,       cmd_rdy);
  Output(SmeshCmd, cmd_bits);
  Input(bit, ld_utilization_at_limit);

  Output(u16, i);
  Output(u16, k);
  Output(bit, idle);
  Output(u8,  loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  struct State {
    LoopMatmulLdAReq req{};
    u16 i = 0;
    u16 k = 0;
    bit active = false;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
