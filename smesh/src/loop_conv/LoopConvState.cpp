// **********************************************************************
// smesh/src/loop_conv/LoopConvState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvState.hpp"
#include "SmeshConfig.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

LoopConvState::LoopConvState(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateView)
      .reads(state_Q_)
      .writes(cmd_rdy, loop_configured, head_loop_id, tail_loop_id, config_loop_id, loop0, loop1);
  UPDATE(updateNextState).reads(state_Q_, decoded, cmd_rdy).writes(state_D_);
}

// Select the head bank for setup unless it is configured; otherwise use the tail.
void LoopConvState::updateView() {
  const auto state = *state_Q_;
  const auto head = static_cast<unsigned>(state.head_id);
  const auto tail = head ^ 1u;
  const auto selected = state.loops[head].configured == 1 ? tail : head;
  head_loop_id = head;
  tail_loop_id = tail;
  config_loop_id = selected;
  cmd_rdy = bit(state.loops[selected].configured == 0);
  loop_configured = bit(state.loops[0].configured == 1 || state.loops[1].configured == 1);
  loop0 = state.loops[0];
  loop1 = state.loops[1];
}

// Save the accepted command's fields for the next clock edge.
void LoopConvState::updateNextState() {
  const auto command = *decoded;
  if (cmd_rdy == 0 || (command.do_config == 0 && command.do_launch == 0)) return;
  const auto current = *state_Q_;
  auto next = current;
  const auto head = static_cast<unsigned>(current.head_id);
  const auto selected = current.loops[head].configured == 1 ? head ^ 1u : head;
  auto& slot = next.loops[selected];
  const auto& outer = command.outer_bounds;
  const auto& inner = command.inner_bounds;
  if (command.do_config == 1) {
    switch (command.config_id) {
      case 1:
        slot.outer_bounds.out_channels = outer.out_channels;
        slot.outer_bounds.in_channels = outer.in_channels;
        slot.outer_bounds.in_row_dim = outer.in_row_dim;
        slot.outer_bounds.batch_size = outer.batch_size;
        slot.outer_bounds.padding = outer.padding;
        slot.outer_bounds.stride = outer.stride;
        slot.outer_bounds.out_col_dim = outer.out_col_dim;
        slot.outer_bounds.pool_out_row_dim = outer.pool_out_row_dim;
        slot.outer_bounds.out_row_dim = outer.out_row_dim;
        break;
      case 2:
        slot.outer_bounds.kernel_dim = outer.kernel_dim;
        slot.outer_bounds.pool_out_col_dim = outer.pool_out_col_dim;
        slot.outer_bounds.pool_size = kDefaultConfig.has_max_pool ? outer.pool_size : 1;
        slot.outer_bounds.pool_stride = kDefaultConfig.has_max_pool ? outer.pool_stride : 1;
        slot.outer_bounds.pool_padding = kDefaultConfig.has_max_pool ? outer.pool_padding : 0;
        slot.inner_bounds.batches = inner.batches;
        slot.inner_bounds.porows = inner.porows;
        slot.inner_bounds.pocols = inner.pocols;
        slot.inner_bounds.pochs = inner.pochs;
        break;
      case 3:
        slot.inner_bounds.krows = inner.krows;
        slot.inner_bounds.kcols = inner.kcols;
        slot.inner_bounds.kchs = inner.kchs;
        slot.inner_bounds.lpad = inner.lpad;
        slot.inner_bounds.rpad = inner.rpad;
        slot.inner_bounds.upad = inner.upad;
        slot.inner_bounds.dpad = inner.dpad;
        slot.inner_bounds.plpad = inner.plpad;
        slot.outer_bounds.in_col_dim = outer.in_col_dim;
        break;
      case 4:
        slot.inner_bounds.orows = inner.orows;
        slot.inner_bounds.prad = inner.prad;
        slot.inner_bounds.pupad = inner.pupad;
        slot.inner_bounds.pdpad = inner.pdpad;
        slot.outer_bounds.kernel_dilation = outer.kernel_dilation;
        slot.inner_bounds.ocols = inner.ocols;
        slot.outer_bounds.in_stride = outer.in_stride;
        slot.outer_bounds.weight_stride = outer.weight_stride;
        slot.outer_bounds.out_stride = outer.out_stride;
        break;
      case 5:
        slot.weights_dram_addr = command.weights_dram_addr;
        slot.output_dram_addr = command.output_dram_addr;
        break;
      case 6:
        slot.bias_dram_addr = command.bias_dram_addr;
        slot.input_dram_addr = command.input_dram_addr;
        break;
      default:
        return;
    }
  } else {
    slot.launch = command.launch;
    // Original disables unsupported options and treats a zero pixel count as one.
    if (!kDefaultConfig.has_training_convs) {
      slot.launch.wrot180 = 0;
      slot.launch.input_dilated = 0;
      slot.launch.trans_output_1203 = 0;
      slot.launch.trans_weight_1203 = 0;
      slot.launch.trans_weight_0132 = 0;
      slot.launch.trans_input_3120 = 0;
    }
    if (!kDefaultConfig.has_dw_convs) slot.launch.dw = 0;
    if (!kDefaultConfig.has_max_pool) slot.launch.no_pool = 1;
    if (!kDefaultConfig.has_first_layer_optimizations || slot.launch.max_pixels_per_row == 0)
      slot.launch.max_pixels_per_row = 1;
    slot.configured = 1;
  }
  state_D_ = next;
  trace("loop_conv state: slot=%u config=%u launch=%u",
        selected, unsigned(command.config_id), unsigned(command.do_launch));
}

// Start with empty banks and assign each its half of SPAD.
void LoopConvState::reset() {
  State state{};
  for (std::size_t id = 0; id < state.loops.size(); ++id) {
    state.loops[id].a_addr_start = id * (kSpRows / 2);
    state.loops[id].b_addr_end = (id + 1) * (kSpRows / 2);
  }
  state_D_.reset(state);
  cmd_rdy.reset(1);
  loop_configured.reset(0);
  head_loop_id.reset(0);
  tail_loop_id.reset(1);
  config_loop_id.reset(0);
  loop0.reset(state.loops[0]);
  loop1.reset(state.loops[1]);
}

} // namespace smesh
