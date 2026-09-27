// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulLdD.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Accepts an D-load request and emits MVIN3 commands to accumulator addresses;
low_d selects the DRAM element widht and DMA block limit.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

// Parameters captured when the D-load generator accepts a loop slot.
struct LoopMatmulLdDReq {
  std::uint16_t max_j = 0;
  std::uint16_t max_i = 0;
  std::uint16_t pad_j = 0;
  std::uint16_t pad_i = 0;
  std::uint64_t dram_addr = 0;
  std::uint64_t dram_stride = 0;
  std::uint32_t addr_start = 0;
  std::uint8_t loop_id = 0;
  bit low_d = false;
};

// Emits MVIN3 commands for the D tiles of one LOOP_WS operation.
class LoopMatmulLdD : public Component {
  DECLARE_COMPONENT(LoopMatmulLdD);

 public:
  LoopMatmulLdD(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,              req_val);
  Output(bit,             req_rdy);
  Input(LoopMatmulLdDReq, req_bits);

  Output(bit,      cmd_val);
  Input(bit,       cmd_rdy);
  Output(SmeshCmd, cmd_bits);
  Input(bit,       ld_utilization_at_limit);

  Output(bit, idle);
  Output(u8,  loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  struct State {
    LoopMatmulLdDReq req{};
    u16 i = 0;
    u16 j = 0;
    bit active = false;
  };

  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
