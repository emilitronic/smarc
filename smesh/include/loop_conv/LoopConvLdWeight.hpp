// **********************************************************************
// smesh/include/loop_conv/LoopConvLdWeight.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Loads convolution weights into the SPAD rows immediately before addr_end.
Emits CONFIG_LOAD (state 1) and LOAD2 through a two-stage elastic pipeline.
Supports Original's ordinary, transposed, and depthwise address calculations.
*/
#pragma once

#include "LoopConvLdWeightReq.hpp"

#include <array>

namespace smesh {

class LoopConvLdWeight : public Component {
  DECLARE_COMPONENT(LoopConvLdWeight);
 public:
  LoopConvLdWeight(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Input(bit,                 req_val);
  Input(LoopConvLdWeightReq, req_bits);
  Output(bit,                req_rdy);
  Output(bit,                cmd_val);
  Output(SmeshCmd,           cmd_bits);
  Input(bit,                 cmd_rdy);
  Input(bit,                 ld_utilization_at_limit);
  Input(bit,                 wait_for_prev_loop);
  Output(bit,                idle);
  Output(u8,                 loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  enum class Phase : std::uint8_t { Idle, Config, Load };
  struct State {
    LoopConvLdWeightReq     req{};
    Phase                   phase = Phase::Idle;
    std::uint16_t           och   = 0;
    std::uint16_t           krow  = 0;
    std::uint16_t           kcol  = 0;
    std::uint16_t           kch   = 0;
    std::array<SmeshCmd, 2> commands{};
    std::array<bit, 2>      valid{};
  };
  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
