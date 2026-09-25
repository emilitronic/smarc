// **********************************************************************
// smesh/src/controllers/st/StCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026

#include "StCtrl.hpp"

#include "StCtrlCmdDec.hpp"
#include "StCtrlCmdTracker.hpp"
#include "StCtrlDmaReq.hpp"
#include "StCtrlGeom.hpp"
#include "StCtrlQueues.hpp"
#include "StCtrlState.hpp"

namespace smesh {

StCtrl::StCtrl(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_ = new StCtrlCmdQueue("CmdQueue");
  decoder_   = new StCtrlCmdDec("CmdDec");
  geometry_  = new StCtrlGeom("Geom");
  request_   = new StCtrlDmaReq("DmaReq");
  tracker_   = new StCtrlCmdTracker("CmdTracker");
  state_     = new StCtrlState("State");

  cmd_queue_->clk << clk;
  decoder_->clk   << clk;
  geometry_->clk  << clk;
  request_->clk   << clk;
  tracker_->clk   << clk;
  state_->clk     << clk;

  cmd_queue_->cmd_val  << cmd_val;
  cmd_queue_->cmd_bits << cmd_bits;
  cmd_rdy << cmd_queue_->cmd_rdy;
  cmd_queue_->head_rdy << state_->head_rdy;

  decoder_->head_val   << cmd_queue_->head_val;
  decoder_->head_bits  << cmd_queue_->head_bits;

  state_->head_val  << cmd_queue_->head_val;
  state_->head_bits << cmd_queue_->head_bits;
  state_->do_config      << decoder_->do_config;
  state_->do_config_norm << decoder_->do_config_norm;
  state_->do_store       << decoder_->do_store;
  state_->rows           << decoder_->rows;
  state_->blocks         << decoder_->blocks;
  state_->localaddr      << decoder_->localaddr;
  state_->pooling_is_enabled << geometry_->pooling_is_enabled;
  state_->mvout_1d_enabled   << geometry_->mvout_1d_enabled;
  state_->mvout_1d_rows      << geometry_->mvout_1d_rows;
  state_->pool_total_rows    << geometry_->pool_total_rows;
  state_->tracker_alloc_rdy    << tracker_->alloc_rdy;
  state_->tracker_alloc_cmd_id << tracker_->alloc_cmd_id;
  state_->dma_req_rdy << dma_req_rdy;

  state_->config_stride            << decoder_->config_stride;
  state_->config_activation        << decoder_->config_activation;
  state_->config_acc_scale         << decoder_->config_acc_scale;
  state_->config_pool_stride       << decoder_->config_pool_stride;
  state_->config_pool_size         << decoder_->config_pool_size;
  state_->config_pool_out_dim      << decoder_->config_pool_out_dim;
  state_->config_porows            << decoder_->config_porows;
  state_->config_pocols            << decoder_->config_pocols;
  state_->config_orows             << decoder_->config_orows;
  state_->config_ocols             << decoder_->config_ocols;
  state_->config_upad              << decoder_->config_upad;
  state_->config_lpad              << decoder_->config_lpad;
  state_->config_stats_id          << decoder_->config_stats_id;
  state_->config_activation_msb    << decoder_->config_activation_msb;
  state_->config_set_stats_id_only << decoder_->config_set_stats_id_only;
  state_->config_iexp_q_const_type << decoder_->config_iexp_q_const_type;
  state_->config_iexp_q_const      << decoder_->config_iexp_q_const;
  state_->config_igelu_qb          << decoder_->config_igelu_qb;
  state_->config_igelu_qc          << decoder_->config_igelu_qc;

  geometry_->vaddr           << decoder_->vaddr;
  geometry_->localaddr       << decoder_->localaddr;
  geometry_->dst_spad_addr   << decoder_->dst_spad_addr;
  geometry_->dst_spad_stride << decoder_->dst_spad_stride;
  geometry_->stride        << state_->stride;
  geometry_->pool_stride   << state_->pool_stride;
  geometry_->pool_size     << state_->pool_size;
  geometry_->pool_out_dim  << state_->pool_out_dim;
  geometry_->pool_porows   << state_->pool_porows;
  geometry_->pool_pocols   << state_->pool_pocols;
  geometry_->pool_orows    << state_->pool_orows;
  geometry_->pool_ocols    << state_->pool_ocols;
  geometry_->pool_upad     << state_->pool_upad;
  geometry_->pool_lpad     << state_->pool_lpad;
  geometry_->row_counter   << state_->row_counter;
  geometry_->block_counter << state_->block_counter;
  geometry_->porow_counter << state_->porow_counter;
  geometry_->pocol_counter << state_->pocol_counter;
  geometry_->wrow_counter  << state_->wrow_counter;
  geometry_->wcol_counter  << state_->wcol_counter;

  request_->dst_is_spad << decoder_->dst_is_spad;
  request_->cols        << decoder_->cols;
  request_->blocks      << decoder_->blocks;
  request_->mstatus     << decoder_->mstatus;
  request_->pooling_is_enabled    << geometry_->pooling_is_enabled;
  request_->mvout_1d_enabled      << geometry_->mvout_1d_enabled;
  request_->current_vaddr         << geometry_->current_vaddr;
  request_->current_localaddr     << geometry_->current_localaddr;
  request_->current_dst_spad_addr << geometry_->current_dst_spad_addr;
  request_->pool_row_addr         << geometry_->pool_row_addr;
  request_->pool_vaddr            << geometry_->pool_vaddr;
  request_->activation    << state_->activation;
  request_->acc_scale     << state_->acc_scale;
  request_->igelu_qb      << state_->igelu_qb;
  request_->igelu_qc      << state_->igelu_qc;
  request_->iexp_qln2     << state_->iexp_qln2;
  request_->iexp_qln2_inv << state_->iexp_qln2_inv;
  request_->norm_stats_id << state_->norm_stats_id;
  request_->block_counter << state_->block_counter;
  request_->wrow_counter  << state_->wrow_counter;
  request_->wcol_counter  << state_->wcol_counter;
  request_->pool_size     << state_->pool_size;
  request_->cmd_id        << state_->dma_req_cmd_id;

  tracker_->alloc_val            << state_->tracker_alloc_val;
  tracker_->alloc_response_count << state_->tracker_alloc_response_count;
  tracker_->alloc_rs_tag         << state_->tracker_alloc_rs_tag;
  tracker_->returned_val            << dma_resp_val;
  tracker_->returned_cmd_id         << returned_cmd_id_;
  tracker_->returned_response_count << returned_response_count_;
  tracker_->completed_rdy           << completed_rdy;

  dma_req_val << state_->dma_req_val;
  dma_req_bits << request_->req_bits;
  dma_resp_rdy   << tracker_->returned_rdy;
  completed_val  << tracker_->completed_val;
  completed_bits << tracker_->completed_bits;
  control_state << state_->control_state;

  UPDATE(updateResponseFields)
      .reads(dma_resp_bits)
      .writes(returned_cmd_id_, returned_response_count_);
}

void StCtrl::updateResponseFields() {
  returned_cmd_id_ = dma_resp_bits->cmd_id;
  returned_response_count_ = 1;
}

void StCtrl::reset() {
  returned_cmd_id_.reset(0);
  returned_response_count_.reset(1);
}

StCtrl::~StCtrl() {
  delete state_;
  delete tracker_;
  delete request_;
  delete geometry_;
  delete decoder_;
  delete cmd_queue_;
}

} // namespace smesh
