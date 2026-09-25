// **********************************************************************
// smesh/src/controllers/ld/LdCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrlState.hpp"

#include "SmeshConfig.hpp"

#include <cassert>

namespace smesh {

TraceKey(ld_ctrl_state_);

LdCtrlState::LdCtrlState(std::string /*name*/, IMPL_CTOR) {
  regs_Q_ <= regs_D_;
  UPDATE(updateView).reads(regs_Q_)
                    .writes(control_state, row_counter, configured, strides,
                            scales, shrinks, block_strides, pixel_repeats);
  UPDATE(updateRequest)
      .reads(regs_Q_, head_val, do_load, tracker_alloc_rdy,
             tracker_alloc_cmd_id)
      .writes(tracker_alloc_val, dma_req_val, dma_req_cmd_id);
  UPDATE(updateTransition)
      .reads(regs_Q_, head_val, do_config, tracker_alloc_val,
             tracker_alloc_rdy, tracker_alloc_cmd_id, dma_req_val,
             dma_req_rdy)
      .reads(actual_rows_read, rows, block_stride, config_state_id,
             config_stride, config_scale, config_shrink,
             config_block_stride)
      .reads(config_pixel_repeats)
      .writes(head_rdy, regs_D_);
}

void LdCtrlState::updateView() {
  const auto q  = *regs_Q_;
  control_state = q.state;
  row_counter   = q.row_counter;
  for (std::size_t i = 0; i < kLoadStates; ++i) {
    configured[i]    = q.configured[i];
    strides[i]       = q.strides[i];
    scales[i]        = q.scales[i];
    shrinks[i]       = q.shrinks[i];
    block_strides[i] = q.block_strides[i];
    pixel_repeats[i] = kDefaultConfig.has_first_layer_optimizations ? q.pixel_repeats[i] : 1;
  }
}

void LdCtrlState::updateRequest() {
  const auto q             = *regs_Q_;
  const auto state         = static_cast<LdCtrlFsmState>(q.state);
  const bool allocating    = state == LdCtrlFsmState::WaitingForCommand &&
                             head_val == 1 && do_load == 1;
  const bool allocated_now = allocating && tracker_alloc_rdy == 1;

  tracker_alloc_val = bit(allocating);
  dma_req_val       = bit(allocated_now ||
                          state == LdCtrlFsmState::WaitingForDmaReqReady ||
                          (state == LdCtrlFsmState::SendingRows && q.row_counter != 0));
  // The first request uses the tracker ID selected in this same cycle.
  dma_req_cmd_id    = allocated_now ? static_cast<std::uint16_t>(*tracker_alloc_cmd_id) : q.cmd_id;
}

void LdCtrlState::updateTransition() {
  const auto q = *regs_Q_;
  auto next = q;
  const auto state = static_cast<LdCtrlFsmState>(q.state);
  const bool req_fire = dma_req_val == 1 && dma_req_rdy == 1;
  head_rdy = 0;

  if (state == LdCtrlFsmState::WaitingForCommand && head_val == 1 && do_config == 1) {
    const auto slot = static_cast<std::size_t>(*config_state_id);
    assert(slot < kLoadStates);
    const auto repeat = static_cast<std::uint8_t>(*config_pixel_repeats);
    assert(kDefaultConfig.has_first_layer_optimizations || repeat <= 1);

    next.strides[slot]       = *config_stride;
    next.scales[slot]        = *config_scale;
    next.shrinks[slot]       = *config_shrink;
    next.block_strides[slot] = *config_block_stride;
    next.pixel_repeats[slot] = kDefaultConfig.has_first_layer_optimizations ? (repeat == 0 ? 1 : repeat) : 1;
    next.configured[slot]    = 1;
    head_rdy = 1;
    trace(ld_ctrl_state_, "config slot=%u\n", static_cast<unsigned>(slot));
  } else if (state == LdCtrlFsmState::WaitingForCommand &&
             tracker_alloc_val == 1 && tracker_alloc_rdy == 1) {
    next.cmd_id = *tracker_alloc_cmd_id;
    next.state  = static_cast<std::uint8_t>(
        req_fire ? LdCtrlFsmState::SendingRows :
                   LdCtrlFsmState::WaitingForDmaReqReady);
    trace(ld_ctrl_state_, "allocate id=%u first_fire=%u\n",
          static_cast<unsigned>(next.cmd_id), static_cast<unsigned>(req_fire));
  } else if (state == LdCtrlFsmState::WaitingForDmaReqReady) {
    if (req_fire) {
      next.state = static_cast<std::uint8_t>(LdCtrlFsmState::SendingRows);
      trace(ld_ctrl_state_, "first_fire id=%u\n", static_cast<unsigned>(q.cmd_id));
    }
  } else if (state == LdCtrlFsmState::SendingRows) {
    const auto count = static_cast<std::uint32_t>(*actual_rows_read);
    const bool last_row = q.row_counter == 0 ||
                          (count != 0 && q.row_counter == count - 1 && req_fire);
    if (last_row) {
      next.state = static_cast<std::uint8_t>(LdCtrlFsmState::WaitingForCommand);
      head_rdy = 1;
      trace(ld_ctrl_state_, "finish id=%u\n", static_cast<unsigned>(q.cmd_id));
    }
  }

  if (req_fire) {
    const auto count = static_cast<std::uint32_t>(*actual_rows_read);
    assert(count != 0);
    assert(block_stride >= rows);
    next.row_counter = q.row_counter + 1 >= count ? 0 : q.row_counter + 1;
  }

  regs_D_ = next;
}

void LdCtrlState::reset() {
  regs_D_.reset(LdCtrlStateRegs{});
  head_rdy.reset(0);
  tracker_alloc_val.reset(0);
  dma_req_val.reset(0);
  dma_req_cmd_id.reset(0);
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
