// **********************************************************************
// smesh/include/loop_conv/LoopConvLdBias.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Accepts one bias-loading job, emits CONFIG_LOAD followed by LOAD3 commands,
and holds commands in a two-stage elastic pipeline until accepted.
Iterator order: output channels, output columns, output rows, batches.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "LoopConvCmdDec.hpp"

#include <array>

namespace smesh {

// Bias worker request; only its two required derived geometry values are included.
struct LoopConvLdBiasReq {
  LoopConvOuterBounds outer_bounds{};
  LoopConvInnerBounds inner_bounds{};
  std::uint16_t       ochs             = 0;
  std::uint16_t       bias_spad_stride = 0;
  std::uint32_t       addr_start       = 0;
  std::uint64_t       dram_addr        = 0;
  bit                 no_bias          = false;
  std::uint8_t        loop_id          = 0;
};

class LoopConvLdBias : public Component {
  DECLARE_COMPONENT(LoopConvLdBias);

 public:
  LoopConvLdBias(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Input(bit,               req_val);
  Input(LoopConvLdBiasReq, req_bits);
  Output(bit,              req_rdy);
  Output(bit,              cmd_val);
  Output(SmeshCmd,         cmd_bits);
  Input(bit,               cmd_rdy);
  Input(bit,               ld_utilization_at_limit);
  Input(bit,               wait_for_prev_loop);
  Output(bit,              idle);
  Output(u8,               loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  enum class Phase : std::uint8_t { Idle, Config, Load };
  struct State {
    LoopConvLdBiasReq req{};
    Phase         phase = Phase::Idle;
    std::uint16_t b     = 0;
    std::uint16_t orow  = 0;
    std::uint16_t ocol  = 0;
    std::uint16_t och   = 0;
    std::array<SmeshCmd, 2> commands{};
    std::array<bit, 2>      valid{};
  };
  Output(State,   state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
