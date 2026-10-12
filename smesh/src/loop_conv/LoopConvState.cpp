// **********************************************************************
// smesh/src/loop_conv/LoopConvState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConvState.hpp"
#include "SmeshConfig.hpp"
#include "SmeshTypes.hpp"

#include <cassert>

namespace smesh {

LoopConvState::LoopConvState(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateView)
      .reads(state_Q_)
      .writes(cmd_rdy, loop_configured, head_loop_id, tail_loop_id, config_loop_id, loop0, loop1);
  UPDATE(updateNextState)
      .reads(state_Q_, decoded, cmd_rdy, ld_bias_req_fire, ld_bias_loop_id)
      .reads(ld_bias_idle, ld_bias_worker_loop_id)
      .reads(ld_input_req_fire, ld_input_loop_id)
      .reads(ld_input_idle, ld_input_worker_loop_id)
      .reads(ld_weights_req_fire, ld_weights_loop_id, ld_weights_idle, ld_weights_worker_loop_id)
      .reads(ex_req_fire, ex_loop_id, ex_idle, ex_worker_loop_id)
      .reads(st_req_fire, st_loop_id, st_idle, st_worker_loop_id)
      .writes(state_D_);
}

// Select the head bank for setup unless it is configured; otherwise use the tail.
void LoopConvState::updateView() {
  const auto state    = *state_Q_;
  const auto head     = static_cast<unsigned>(state.head_id);
  const auto tail     = head ^ 1u;
  const auto selected = state.loops[head].configured == 1 ? tail : head;
  head_loop_id        = head;
  tail_loop_id        = tail;
  config_loop_id      = selected;
  cmd_rdy             = bit(state.loops[selected].configured == 0);
  loop_configured     = bit(state.loops[0].configured == 1 || state.loops[1].configured == 1);
  loop0               = state.loops[0];
  loop1               = state.loops[1];
}

// Save accepted setup and mark accepted worker requests started at the next edge.
void LoopConvState::updateNextState() {
  const auto command        = *decoded;
  const bool setup          = cmd_rdy == 1 && (command.do_config == 1 || command.do_launch == 1);
  const auto current        = *state_Q_;
  const auto worker_id      = static_cast<unsigned>(*ld_bias_worker_loop_id);
  assert(worker_id < current.loops.size());
  const auto& worker_slot   = current.loops[worker_id];
  const bool bias_finished  = ld_bias_idle == 1 && worker_slot.running == 1 && worker_slot.ld_bias_started == 1 && worker_slot.ld_bias_completed == 0;
  const auto input_id       = static_cast<unsigned>(*ld_input_worker_loop_id);
  assert(input_id < current.loops.size());
  const auto& input_slot    = current.loops[input_id];
  const bool input_finished = ld_input_idle == 1 && input_slot.running == 1 && input_slot.ld_input_started == 1 && input_slot.ld_input_completed == 0;
  const auto weights_id = static_cast<unsigned>(*ld_weights_worker_loop_id);
  assert(weights_id < current.loops.size());
  const auto& weights_slot = current.loops[weights_id];
  const bool weights_finished = ld_weights_idle == 1 && weights_slot.running == 1 && weights_slot.ld_weights_started == 1 && weights_slot.ld_weights_completed == 0;
  const auto ex_id = static_cast<unsigned>(*ex_worker_loop_id);
  assert(ex_id < current.loops.size());
  const auto& ex_slot = current.loops[ex_id];
  const bool ex_finished = ex_idle == 1 && ex_slot.running == 1 && ex_slot.ex_started == 1 && ex_slot.ex_completed == 0;
  const auto st_id = static_cast<unsigned>(*st_worker_loop_id);
  assert(st_id < current.loops.size());
  const auto& st_slot = current.loops[st_id];
  const bool st_finished = st_idle == 1 && st_slot.running == 1 && st_slot.st_started == 1 && st_slot.st_completed == 0;
  if (!setup && ld_bias_req_fire == 0 && ld_input_req_fire == 0 && ld_weights_req_fire == 0 && ex_req_fire == 0 && st_req_fire == 0 &&
      !bias_finished && !input_finished && !weights_finished && !ex_finished && !st_finished) return;
  auto next           = current;
  const auto head     = static_cast<unsigned>(current.head_id);
  const auto selected = current.loops[head].configured == 1 ? head ^ 1u : head;
  if (setup) {
    auto& slot        = next.loops[selected];
    const auto& outer = command.outer_bounds;
    const auto& inner = command.inner_bounds;
    if (command.do_config == 1) {
      switch (command.config_id) {
        case 1:
          slot.outer_bounds.out_channels     = outer.out_channels;
          slot.outer_bounds.in_channels      = outer.in_channels;
          slot.outer_bounds.in_row_dim       = outer.in_row_dim;
          slot.outer_bounds.batch_size       = outer.batch_size;
          slot.outer_bounds.padding          = outer.padding;
          slot.outer_bounds.stride           = outer.stride;
          slot.outer_bounds.out_col_dim      = outer.out_col_dim;
          slot.outer_bounds.pool_out_row_dim = outer.pool_out_row_dim;
          slot.outer_bounds.out_row_dim      = outer.out_row_dim;
          break;
        case 2:
          slot.outer_bounds.kernel_dim       = outer.kernel_dim;
          slot.outer_bounds.pool_out_col_dim = outer.pool_out_col_dim;
          slot.outer_bounds.pool_size        = kDefaultConfig.has_max_pool ? outer.pool_size : 1;
          slot.outer_bounds.pool_stride      = kDefaultConfig.has_max_pool ? outer.pool_stride : 1;
          slot.outer_bounds.pool_padding     = kDefaultConfig.has_max_pool ? outer.pool_padding : 0;
          slot.inner_bounds.batches          = inner.batches;
          slot.inner_bounds.porows           = inner.porows;
          slot.inner_bounds.pocols           = inner.pocols;
          slot.inner_bounds.pochs            = inner.pochs;
          break;
        case 3:
          slot.inner_bounds.krows      = inner.krows;
          slot.inner_bounds.kcols      = inner.kcols;
          slot.inner_bounds.kchs       = inner.kchs;
          slot.inner_bounds.lpad       = inner.lpad;
          slot.inner_bounds.rpad       = inner.rpad;
          slot.inner_bounds.upad       = inner.upad;
          slot.inner_bounds.dpad       = inner.dpad;
          slot.inner_bounds.plpad      = inner.plpad;
          slot.outer_bounds.in_col_dim = outer.in_col_dim;
          break;
        case 4:
          slot.inner_bounds.orows           = inner.orows;
          slot.inner_bounds.prad            = inner.prad;
          slot.inner_bounds.pupad           = inner.pupad;
          slot.inner_bounds.pdpad           = inner.pdpad;
          slot.outer_bounds.kernel_dilation = outer.kernel_dilation;
          slot.inner_bounds.ocols           = inner.ocols;
          slot.outer_bounds.in_stride       = outer.in_stride;
          slot.outer_bounds.weight_stride   = outer.weight_stride;
          slot.outer_bounds.out_stride      = outer.out_stride;
          break;
        case 5:
          slot.weights_dram_addr = command.weights_dram_addr;
          slot.output_dram_addr  = command.output_dram_addr;
          break;
        case 6:
          slot.bias_dram_addr  = command.bias_dram_addr;
          slot.input_dram_addr = command.input_dram_addr;
          break;
        default:
          return;
      }
    } else {
      slot.launch = command.launch;
      // Original disables unsupported options and treats a zero pixel count as one.
      if (!kDefaultConfig.has_training_convs) {
        slot.launch.wrot180           = 0;
        slot.launch.input_dilated     = 0;
        slot.launch.trans_output_1203 = 0;
        slot.launch.trans_weight_1203 = 0;
        slot.launch.trans_weight_0132 = 0;
        slot.launch.trans_input_3120  = 0;
      }
      if (!kDefaultConfig.has_dw_convs) slot.launch.dw      = 0;
      if (!kDefaultConfig.has_max_pool) slot.launch.no_pool = 1;
      if (!kDefaultConfig.has_first_layer_optimizations || slot.launch.max_pixels_per_row == 0) slot.launch.max_pixels_per_row = 1;
      slot.configured = 1;
    }
    trace("loop_conv state: slot=%u config=%u launch=%u", selected, unsigned(command.config_id), unsigned(command.do_launch));
  }
  // A worker starts on request acceptance, independently of setup in the other slot.
  if (ld_bias_req_fire == 1) {
    const auto id = static_cast<unsigned>(*ld_bias_loop_id);
    assert(id < next.loops.size());
    assert(current.loops[id].configured == 1 && current.loops[id].ld_bias_started == 0);
    next.loops[id].running         = 1;
    next.loops[id].ld_bias_started = 1;
    trace("loop_conv state: bias started slot=%u", id);
  }
  // Idle includes pipeline drain: every generated bias command has left the worker.
  // This records command-generation completion, not RS execution completion.
  if (bias_finished) {
    next.loops[worker_id].ld_bias_completed = 1;
    trace("loop_conv state: bias finished slot=%u", worker_id);
  }
  if (ld_input_req_fire == 1) {
    const auto id = static_cast<unsigned>(*ld_input_loop_id);
    assert(id < next.loops.size());
    assert(current.loops[id].configured == 1 && current.loops[id].ld_input_started == 0);
    next.loops[id].running          = 1;
    next.loops[id].ld_input_started = 1;
    trace("loop_conv state: input started slot=%u", id);
  }
  // All input commands must leave the worker before generation is complete.
  if (input_finished) {
    next.loops[input_id].ld_input_completed = 1;
    trace("loop_conv state: input finished slot=%u", input_id);
  }
  if (ld_weights_req_fire == 1) {
    const auto id = static_cast<unsigned>(*ld_weights_loop_id);
    assert(id < next.loops.size());
    assert(current.loops[id].configured == 1 && current.loops[id].ld_weights_started == 0);
    next.loops[id].running            = 1;
    next.loops[id].ld_weights_started = 1;
    trace("loop_conv state: weights started slot=%u", id);
  }
  // Idle includes both command-pipeline stages draining, not memory execution.
  if (weights_finished) {
    next.loops[weights_id].ld_weights_completed = 1;
    trace("loop_conv state: weights finished slot=%u", weights_id);
  }
  if (ex_req_fire == 1) {
    const auto id = static_cast<unsigned>(*ex_loop_id);
    assert(id < next.loops.size());
    const auto& slot = current.loops[id];
    assert(slot.configured == 1 && slot.ex_started == 0 && slot.ld_bias_started == 1 &&
           slot.ld_input_started == 1 && slot.ld_weights_started == 1);
    next.loops[id].running = 1;
    next.loops[id].ex_started = 1;
    trace("loop_conv state: execute started slot=%u", id);
  }
  if (ex_finished) {
    next.loops[ex_id].ex_completed = 1;
    trace("loop_conv state: execute finished slot=%u", ex_id);
  }
  if (st_req_fire == 1) {
    const auto id = static_cast<unsigned>(*st_loop_id);
    assert(id < next.loops.size());
    assert(current.loops[id].configured == 1 && current.loops[id].st_started == 0 && current.loops[id].ex_started == 1);
    next.loops[id].running = 1;
    next.loops[id].st_started = 1;
    trace("loop_conv state: store started slot=%u", id);
  }
  // Completion here is command-generation/pipeline drain, not DMA completion.
  if (st_finished) {
    next.loops[st_id].st_completed = 1;
    trace("loop_conv state: store finished slot=%u", st_id);
  }
  state_D_ = next;
}

// Start with empty banks and assign each its half of SPAD.
void LoopConvState::reset() {
  State state{};
  for (std::size_t id = 0; id < state.loops.size(); ++id) {
    state.loops[id].a_addr_start = id * (kSpRows / 2);
    state.loops[id].b_addr_end   = (id + 1) * (kSpRows / 2);
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
