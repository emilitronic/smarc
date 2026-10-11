// **********************************************************************
// smesh/include/loop_conv/LoopConvCmdDec.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Combinational extraction of Original's six LOOP_CONV_WS configuration commands
and launch command. This block holds no configuration or loop-bank state.
Fields are raw command values; feature overrides and compatibility defaults
are applied when loop-bank state accepts them.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

// Whole-convolution dimensions and strides supplied by configurations 1..4.
struct LoopConvOuterBounds {
  std::uint16_t batch_size       = 0;
  std::uint16_t in_row_dim       = 0;
  std::uint16_t in_col_dim       = 0;
  std::uint16_t in_channels      = 0;
  std::uint16_t out_channels     = 0;
  std::uint16_t out_col_dim      = 0;
  std::uint16_t out_row_dim      = 0;
  std::uint16_t out_stride       = 0;
  std::uint16_t in_stride        = 0;
  std::uint16_t weight_stride    = 0;
  std::uint16_t pool_out_row_dim = 0;
  std::uint16_t pool_out_col_dim = 0;
  std::uint16_t stride           = 0;
  std::uint16_t padding          = 0;
  std::uint16_t kernel_dim       = 0;
  std::uint16_t kernel_dilation  = 0;
  std::uint16_t pool_size        = 0;
  std::uint16_t pool_stride      = 0;
  std::uint16_t pool_padding     = 0;
};

// Bounds and padding for the current convolution portion, from configs 2..4.
struct LoopConvInnerBounds {
  std::uint16_t batches = 0;
  std::uint16_t porows  = 0;
  std::uint16_t pocols  = 0;
  std::uint16_t pochs   = 0;
  std::uint16_t krows   = 0;
  std::uint16_t kcols   = 0;
  std::uint16_t kchs    = 0;
  std::uint16_t lpad    = 0;
  std::uint16_t rpad    = 0;
  std::uint16_t upad    = 0;
  std::uint16_t dpad    = 0;
  std::uint16_t plpad   = 0;
  std::uint16_t prad    = 0;
  std::uint16_t pupad   = 0;
  std::uint16_t pdpad   = 0;
  std::uint16_t orows   = 0;
  std::uint16_t ocols   = 0;
};

// Raw launch options; this packet does not itself start a worker.
struct LoopConvLaunch {
  bit no_bias                     = false;
  bit wrot180                     = false;
  bit trans_output_1203           = false;
  bit trans_weight_1203           = false;
  bit trans_weight_0132           = false;
  bit trans_input_3120            = false;
  bit dw                          = false;
  bit no_pool                     = false;
  bit downsample                  = false;
  bit input_dilated               = false;
  std::uint8_t activation         = 0;
  std::uint8_t max_pixels_per_row = 0;
  std::uint8_t a_ex_spad_id       = 0;
  std::uint8_t b_ex_spad_id       = 0;
};

// Fields supplied by one queue-head command, not a saved loop configuration.
// config_id identifies which subset the loop bank should update (1..6).
struct LoopConvDecodedCmd {
  bit                 do_config = false;
  bit                 do_launch = false;
  std::uint8_t        config_id = 0;
  LoopConvOuterBounds outer_bounds{};
  LoopConvInnerBounds inner_bounds{};
  LoopConvLaunch      launch{};
  std::uint64_t       weights_dram_addr = 0;
  std::uint64_t       output_dram_addr = 0;
  std::uint64_t       bias_dram_addr = 0;
  std::uint64_t       input_dram_addr = 0;
};

class LoopConvCmdDec : public Component {
  DECLARE_COMPONENT(LoopConvCmdDec);

 public:
  LoopConvCmdDec(std::string name, COMPONENT_CTOR);
  Clock(clk);

  Input(bit,                 head_val);
  Input(SmeshQueuedCmd,      head_bits);
  Output(LoopConvDecodedCmd, decoded);

  void update();
  void reset();
};

} // namespace smesh
