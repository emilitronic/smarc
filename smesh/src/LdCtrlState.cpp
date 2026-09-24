// **********************************************************************
// smesh/src/LdCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrlState.hpp"

#include "SmeshConfig.hpp"

#include <cassert>

namespace smesh {

LdCtrlState::LdCtrlState(std::string /*name*/, IMPL_CTOR) {
  regs_Q_ <= regs_D_;
  UPDATE(updateView).reads(regs_Q_)
                    .writes(control_state, row_counter, configured, strides,
                            scales, shrinks, block_strides, pixel_repeats);
  UPDATE(updateConfig).reads(regs_Q_, head_val, do_config, config_state_id,
                             config_stride, config_scale, config_shrink,
                             config_block_stride)
                      .reads(config_pixel_repeats)
                      .writes(head_rdy, regs_D_);
}

void LdCtrlState::updateView() {
  const auto q = *regs_Q_;
  control_state = q.state;
  row_counter = q.row_counter;
  for (std::size_t i = 0; i < kLoadStates; ++i) {
    configured[i] = q.configured[i];
    strides[i] = q.strides[i];
    scales[i] = q.scales[i];
    shrinks[i] = q.shrinks[i];
    block_strides[i] = q.block_strides[i];
    pixel_repeats[i] = kDefaultConfig.has_first_layer_optimizations
                           ? q.pixel_repeats[i] : 1;
  }
}

void LdCtrlState::updateConfig() {
  const auto q = *regs_Q_;
  auto next = q;
  head_rdy = 0;

  if (q.state == static_cast<std::uint8_t>(LdCtrlFsmState::WaitingForCommand) &&
      head_val == 1 && do_config == 1) {
    const auto slot = static_cast<std::size_t>(*config_state_id);
    assert(slot < kLoadStates);
    const auto repeat = static_cast<std::uint8_t>(*config_pixel_repeats);
    assert(kDefaultConfig.has_first_layer_optimizations || repeat <= 1);

    next.strides[slot] = *config_stride;
    next.scales[slot] = *config_scale;
    next.shrinks[slot] = *config_shrink;
    next.block_strides[slot] = *config_block_stride;
    next.pixel_repeats[slot] = kDefaultConfig.has_first_layer_optimizations
                                   ? (repeat == 0 ? 1 : repeat) : 1;
    next.configured[slot] = 1;
    head_rdy = 1;
  }

  regs_D_ = next;
}

void LdCtrlState::reset() {
  regs_D_.reset(LdCtrlStateRegs{});
  head_rdy.reset(0);
  control_state.reset(static_cast<std::uint8_t>(LdCtrlFsmState::WaitingForCommand));
  row_counter.reset(0);
  for (std::size_t i = 0; i < kLoadStates; ++i) {
    configured[i].reset(0);
    strides[i].reset(0);
    scales[i].reset(0);
    shrinks[i].reset(0);
    block_strides[i].reset(0);
    pixel_repeats[i].reset(1);
  }
}

} // namespace smesh
