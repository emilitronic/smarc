// **********************************************************************
// smesh/include/loop_conv/LoopConvState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Owns two saved LoopConv configurations and selects the bank receiving setup.
Configuration commands update their own fields; launch marks that bank configured.
Bias request acceptance marks its worker started; retirement comes later.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "LoopConvCmdDec.hpp"

#include <array>

namespace smesh {

// One saved convolution setup, its local-memory region, and worker progress.
struct LoopConvSlot {
  LoopConvOuterBounds outer_bounds{};
  LoopConvInnerBounds inner_bounds{};
  LoopConvLaunch launch{};
  std::uint64_t weights_dram_addr = 0;
  std::uint64_t output_dram_addr = 0;
  std::uint64_t bias_dram_addr = 0;
  std::uint64_t input_dram_addr = 0;
  std::uint32_t a_addr_start = 0;
  std::uint32_t b_addr_end = 0;
  bit configured = false;
  bit running = false;
  bit ld_bias_started = false;
  bit ld_input_started = false;
  bit ld_weights_started = false;
  bit ex_started = false;
  bit st_started = false;
  bit ld_bias_completed = false;
  bit ld_input_completed = false;
  bit ld_weights_completed = false;
  bit ex_completed = false;
  bit st_completed = false;
};

class LoopConvState : public Component {
  DECLARE_COMPONENT(LoopConvState);

 public:
  LoopConvState(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Input(LoopConvDecodedCmd, decoded);
  Input(bit, ld_bias_req_fire);
  Input(u8, ld_bias_loop_id);
  Output(bit, cmd_rdy);
  Output(bit, loop_configured);
  Output(u8, head_loop_id);
  Output(u8, tail_loop_id);
  Output(u8, config_loop_id);
  Output(LoopConvSlot, loop0);
  Output(LoopConvSlot, loop1);

  void updateView();
  void updateNextState();
  void reset();

 private:
  struct State {
    std::array<LoopConvSlot, 2> loops{};
    u8 head_id = 0;
  };
  Output(State, state_Q_);
  Register(State, state_D_);
};

} // namespace smesh
