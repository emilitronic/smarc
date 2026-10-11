// **********************************************************************
// smesh/src/loop_conv/LoopConvCmdDec.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvCmdDec.hpp"
#include "SmeshCommand.hpp"

namespace smesh {
namespace {

// Extract a raw unsigned command field; all callers use widths below 64.
constexpr std::uint16_t field(std::uint64_t word, unsigned low, unsigned width) {
  return static_cast<std::uint16_t>((word >> low) & ((std::uint64_t{1} << width) - 1));
}

} // namespace

LoopConvCmdDec::LoopConvCmdDec(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(head_val, head_bits).writes(decoded);
}

// Decode only the current valid command; unrelated and invalid heads yield zero.
void LoopConvCmdDec::update() {
  LoopConvDecodedCmd value{};
  if (head_val == 1) {
    const auto head = *head_bits;
    const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(head.cmd.funct));
    const auto rs1 = static_cast<std::uint64_t>(head.cmd.rs1);
    const auto rs2 = static_cast<std::uint64_t>(head.cmd.rs2);
    switch (funct) {
      case SmeshFunct::LoopConvWsConfig1:
        value.do_config                     = 1;
        value.config_id                     = 1;
        value.outer_bounds.out_channels     = field(rs1, 48, 16);
        value.outer_bounds.in_channels      = field(rs1, 32, 16);
        value.outer_bounds.in_row_dim       = field(rs1, 16, 16);
        value.outer_bounds.batch_size       = field(rs1, 0, 16);
        value.outer_bounds.padding          = field(rs2, 56, 8);
        value.outer_bounds.stride           = field(rs2, 48, 8);
        value.outer_bounds.out_col_dim      = field(rs2, 32, 16);
        value.outer_bounds.pool_out_row_dim = field(rs2, 16, 16);
        value.outer_bounds.out_row_dim      = field(rs2, 0, 16);
        break;
      case SmeshFunct::LoopConvWsConfig2:
        value.do_config                     = 1;
        value.config_id                     = 2;
        value.outer_bounds.kernel_dim       = field(rs1, 48, 16);
        value.outer_bounds.pool_out_col_dim = field(rs1, 32, 16);
        value.outer_bounds.pool_size        = field(rs1, 16, 16);
        value.outer_bounds.pool_stride      = field(rs1, 8, 8);
        value.outer_bounds.pool_padding     = field(rs1, 0, 8);
        value.inner_bounds.batches          = field(rs2, 48, 16);
        value.inner_bounds.porows           = field(rs2, 32, 16);
        value.inner_bounds.pocols           = field(rs2, 16, 16);
        value.inner_bounds.pochs            = field(rs2, 0, 16);
        break;
      case SmeshFunct::LoopConvWsConfig3:
        value.do_config                     = 1;
        value.config_id                     = 3;
        value.inner_bounds.krows            = field(rs1, 48, 16);
        value.inner_bounds.kcols            = field(rs1, 32, 16);
        value.inner_bounds.kchs             = field(rs1, 16, 16);
        value.inner_bounds.lpad             = field(rs1, 0, 16);
        value.inner_bounds.rpad             = field(rs2, 48, 16);
        value.inner_bounds.upad             = field(rs2, 32, 16);
        value.inner_bounds.dpad             = field(rs2, 24, 8);
        value.inner_bounds.plpad            = field(rs2, 16, 8);
        value.outer_bounds.in_col_dim       = field(rs2, 0, 16);
        break;
      case SmeshFunct::LoopConvWsConfig4:
        value.do_config                     = 1;
        value.config_id                     = 4;
        value.inner_bounds.orows            = field(rs1, 48, 16);
        value.inner_bounds.prad             = field(rs1, 32, 16);
        value.inner_bounds.pupad            = field(rs1, 21, 11);
        value.inner_bounds.pdpad            = field(rs1, 10, 11);
        value.outer_bounds.kernel_dilation  = field(rs1, 0, 10);
        value.inner_bounds.ocols            = field(rs2, 0, 16);
        value.outer_bounds.in_stride        = field(rs2, 48, 16);
        value.outer_bounds.weight_stride    = field(rs2, 32, 16);
        value.outer_bounds.out_stride       = field(rs2, 16, 16);
        break;
      case SmeshFunct::LoopConvWsConfig5:
        value.do_config         = 1;
        value.config_id         = 5;
        value.weights_dram_addr = rs1;
        value.output_dram_addr  = rs2;
        break;
      case SmeshFunct::LoopConvWsConfig6:
        value.do_config       = 1;
        value.config_id       = 6;
        value.bias_dram_addr  = rs1;
        value.input_dram_addr = rs2;
        break;
      case SmeshFunct::LoopConvWs:
        value.do_launch                 = 1;
        value.launch.no_bias            = bit(field(rs1, 0, 1));
        value.launch.wrot180            = bit(field(rs1, 1, 1));
        value.launch.trans_output_1203  = bit(field(rs1, 2, 1));
        value.launch.trans_weight_1203  = bit(field(rs1, 3, 1));
        value.launch.trans_weight_0132  = bit(field(rs1, 4, 1));
        value.launch.trans_input_3120   = bit(field(rs1, 5, 1));
        value.launch.dw                 = bit(field(rs1, 6, 1));
        value.launch.no_pool            = bit(field(rs2, 0, 1));
        value.launch.downsample         = bit(field(rs2, 1, 1));
        value.launch.input_dilated      = bit(field(rs2, 2, 1));
        value.launch.max_pixels_per_row = field(rs1, 8, 8);
        value.launch.a_ex_spad_id       = field(rs1, 18, 2);
        value.launch.b_ex_spad_id       = field(rs1, 16, 2);
        value.launch.activation         = field(rs2, 3, 2);
        break;
      default:
        break;
    }
  }
  decoded = value;
}

void LoopConvCmdDec::reset() {
  decoded.reset(LoopConvDecodedCmd{});
}

} // namespace smesh
