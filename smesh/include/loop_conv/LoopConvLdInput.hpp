// **********************************************************************
// smesh/include/loop_conv/LoopConvLdInput.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Loads convolution input into SPAD, including zero padding and layout options.
Emits CONFIG_LOAD and LOAD through a two-stage elastic command pipeline.
*/
#pragma once

#include "LoopConvCmdDec.hpp"

#include <array>

namespace smesh {

// Derived input dimensions and SPAD block stride; each field is 16 bits in Original.
struct LoopConvInputGeometry {
  std::uint16_t irows             = 0;
  std::uint16_t icols             = 0;
  std::uint16_t irows_unpadded    = 0;
  std::uint16_t icols_unpadded    = 0;
  std::uint16_t ichs              = 0;
  std::uint16_t input_spad_stride = 0;
};

// One loop slot's input-loading job, including layout and sampling options.
struct LoopConvLdInputReq {
  LoopConvOuterBounds   outer_bounds{};
  LoopConvInnerBounds   inner_bounds{};
  LoopConvInputGeometry geometry{};
  std::uint32_t         addr_start         = 0;
  std::uint64_t         dram_addr          = 0;
  bit                   downsample         = false;
  std::uint16_t         max_pixels_per_row = 0;
  bit                   input_dilated      = false;
  bit                   trans_input_3120   = false;
  std::uint8_t          loop_id            = 0;
};

class LoopConvLdInput : public Component {
  DECLARE_COMPONENT(LoopConvLdInput);

 public:
  LoopConvLdInput(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Input(bit,                req_val);
  Input(LoopConvLdInputReq, req_bits);
  Output(bit,               req_rdy);
  Output(bit,               cmd_val);
  Output(SmeshCmd,          cmd_bits);
  Input(bit,                cmd_rdy);
  Input(bit,                ld_utilization_at_limit);
  Input(bit,                wait_for_prev_loop);
  Output(bit,               idle);
  Output(u8,      loop_id);

  void updateStatus();
  void updateCommand();
  void updateNextState();
  void reset();

 private:
  enum class Phase : std::uint8_t { Idle, Config, Load };
  struct State {
    LoopConvLdInputReq      req{};
    Phase                   phase = Phase::Idle;
    std::int16_t            b     = 0;
    std::int16_t            irow  = 0;
    std::int16_t            icol  = 0;
    std::int16_t            ich   = 0;
    std::array<SmeshCmd, 2> commands{};
    std::array<bit, 2>      valid{};
  };
  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
