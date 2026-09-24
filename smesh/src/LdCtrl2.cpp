// **********************************************************************
// smesh/include/LdCtrl2.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrl2.hpp"

#include "LdCtrlCmdDec.hpp"
#include "LdCtrlCmdTracker.hpp"
#include "LdCtrlDmaReq.hpp"
#include "LdCtrlGeom.hpp"
#include "LdCtrlQueues.hpp"
#include "LdCtrlState.hpp"

#include <cassert>

namespace smesh {

LdCtrl2::LdCtrl2(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_ = new LdCtrlCmdQueue("CmdQueue");
  decoder_   = new LdCtrlCmdDec("CmdDec");
  state_     = new LdCtrlState("State");
  geometry_  = new LdCtrlGeom("Geom");
  request_   = new LdCtrlDmaReq("DmaReq");
  tracker_   = new LdCtrlCmdTracker("CmdTracker");

  cmd_queue_->clk << clk;
  decoder_->clk   << clk;
  state_->clk     << clk;
  geometry_->clk  << clk;
  request_->clk   << clk;
  tracker_->clk   << clk;

  cmd_queue_->cmd_val  << cmd_val;
  cmd_queue_->cmd_bits << cmd_bits;
  cmd_rdy              << cmd_queue_->cmd_rdy;
  cmd_queue_->head_rdy << state_->head_rdy;

  decoder_->head_val  << cmd_queue_->head_val;
  decoder_->head_bits << cmd_queue_->head_bits;

  state_->head_val             << cmd_queue_->head_val;
  state_->do_config            << decoder_->do_config;
  state_->do_load              << decoder_->do_load;
  state_->rows                 << decoder_->rows;
  state_->block_stride         << block_stride_;
  state_->config_state_id      << decoder_->config_state_id;
  state_->config_stride        << decoder_->config_stride;
  state_->config_scale         << decoder_->config_scale;
  state_->config_shrink        << decoder_->config_shrink;
  state_->config_block_stride  << decoder_->config_block_stride;
  state_->config_pixel_repeats << decoder_->config_pixel_repeats;

  state_->tracker_alloc_rdy    << tracker_->alloc_rdy;
  state_->tracker_alloc_cmd_id << tracker_->alloc_cmd_id;
  state_->dma_req_rdy          << dma_req_rdy;
  state_->actual_rows_read     << geometry_->actual_rows_read;

  geometry_->vaddr       << decoder_->vaddr;
  geometry_->localaddr   << decoder_->localaddr;
  geometry_->rows        << decoder_->rows;
  geometry_->stride      << stride_;
  geometry_->row_counter << state_->row_counter;

  request_->current_vaddr     << geometry_->current_vaddr;
  request_->current_localaddr << geometry_->localaddr_plus_row_counter;
  request_->cols              << decoder_->cols;
  request_->rows              << decoder_->rows;
  request_->actual_rows_read  << geometry_->actual_rows_read;
  request_->stride            << stride_;
  request_->all_zeros         << geometry_->all_zeros;
  request_->scale             << scale_;
  request_->shrink            << shrink_;
  request_->block_stride      << block_stride_;
  request_->pixel_repeat      << pixel_repeat_;
  request_->cmd_id            << state_->dma_req_cmd_id;

  tracker_->alloc_val           << state_->tracker_alloc_val;
  tracker_->alloc_bytes_to_read << request_->bytes_to_read;
  tracker_->alloc_rs_tag        << alloc_rs_tag_;
  tracker_->returned_val        << dma_resp_val;
  tracker_->returned_cmd_id     << returned_cmd_id_;
  tracker_->returned_bytes_read << returned_bytes_read_;
  tracker_->completed_rdy       << completed_rdy;

  dma_req_val    << state_->dma_req_val;
  dma_req_bits   << request_->req_bits;
  completed_val  << tracker_->completed_val;
  completed_bits << tracker_->completed_bits;
  control_state  << state_->control_state;

  UPDATE(updateSelectedConfig)
      .reads(decoder_->state_id, state_->strides, state_->scales,
             state_->shrinks, state_->block_strides, state_->pixel_repeats)
      .writes(stride_, scale_, shrink_, block_stride_, pixel_repeat_);
  UPDATE(updateHeadTag).reads(cmd_queue_->head_bits).writes(alloc_rs_tag_);
  UPDATE(updateReturnFields).reads(dma_resp_bits)
                            .writes(returned_cmd_id_, returned_bytes_read_);
  UPDATE(updateBusy).reads(cmd_queue_->head_val, tracker_->busy).writes(busy);
}

void LdCtrl2::updateSelectedConfig() {
  const auto slot = static_cast<std::size_t>(*decoder_->state_id);
  assert(slot < kLoadStates);
  stride_       = *state_->strides[slot];
  scale_        = *state_->scales[slot];
  shrink_       = *state_->shrinks[slot];
  block_stride_ = *state_->block_strides[slot];
  pixel_repeat_ = *state_->pixel_repeats[slot];
}

void LdCtrl2::updateHeadTag() {
  alloc_rs_tag_ = cmd_queue_->head_bits->rs_tag;
}

void LdCtrl2::updateReturnFields() {
  returned_cmd_id_ = dma_resp_bits->cmd_id;
  returned_bytes_read_ = dma_resp_bits->bytes_read;
}

void LdCtrl2::updateBusy() {
  busy = bit(cmd_queue_->head_val == 1 || tracker_->busy == 1);
}

void LdCtrl2::reset() {
  stride_.reset(0);
  scale_.reset(0);
  shrink_.reset(0);
  block_stride_.reset(0);
  pixel_repeat_.reset(1);
  alloc_rs_tag_.reset(0);
  returned_cmd_id_.reset(0);
  returned_bytes_read_.reset(0);
  busy.reset(0);
}

LdCtrl2::~LdCtrl2() {
  delete tracker_;
  delete request_;
  delete geometry_;
  delete state_;
  delete decoder_;
  delete cmd_queue_;
}

} // namespace smesh
