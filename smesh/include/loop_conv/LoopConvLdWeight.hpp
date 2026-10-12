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

#include "LoopConvCmdDec.hpp"

#include <array>

namespace smesh {

// Weight-layout fields consumed by the worker; Original stores each in 16 bits.
// The *_per_bank names mean DIM-sized channel groups, not physical SPAD banks.
struct LoopConvWeightGeometry {
  std::uint16_t ochs                  = 0;
  std::uint16_t out_channels_per_bank = 0;
  std::uint16_t in_channels_per_bank  = 0;
  std::uint16_t weight_spad_stride    = 0;
};

// One loop slot's weight-loading job. Weights occupy rows immediately before
// addr_end, an exclusive SPAD end address which can equal the total row count.
struct LoopConvLdWeightReq {
  LoopConvOuterBounds    outer_bounds{};
  LoopConvInnerBounds    inner_bounds{};
  LoopConvWeightGeometry geometry{};
  std::uint32_t          addr_end          = 0;
  std::uint64_t          dram_addr         = 0;
  bit                    trans_weight_1203 = false;
  bit                    trans_weight_0132 = false;
  bit                    dw                = false;
  std::uint8_t           loop_id           = 0;
};

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
