// **********************************************************************
// smesh/src/ExCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026

#include "ExCtrl.hpp"

namespace smesh {

ExCtrl::ExCtrl(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_   = new ExCtrlCmdQueue("ExCtrlCmdQueue");
  completion_  = new ExCtrlCompletion("ExCtrlCompletion");
  cmd_decoder_ = new ExCtrlDecoder("ExCtrlDecoder");
  cmd_state_   = new ExCtrlState("ExCtrlState");
  cmd_rowaddr_ = new ExCtrlRowAddr("ExCtrlRowAddr");
  cmd_rowpad_  = new ExCtrlRowPad("ExCtrlRowPad");

  cmd_queue_->clk   << clk;
  completion_->clk  << clk;
  cmd_decoder_->clk << clk;
  cmd_state_->clk   << clk;
  cmd_rowaddr_->clk << clk;
  cmd_rowpad_->clk  << clk;
  
  cmd_queue_->cmd_in    << cmd_in;
  cmd_queue_->pop_count << cmd_state_->cmd_pop_count;
  // get cmd queue head data into decoder and FSM, and pass some decoder o/p to FSM
  for (std::size_t i = 0; i < kExCtrlCmdWindow; ++i) {
    cmd_decoder_->head_val[i]  << cmd_queue_->head_val[i];
    cmd_decoder_->head_bits[i] << cmd_queue_->head_bits[i];
    cmd_state_->head_val[i]    << cmd_queue_->head_val[i];
    cmd_state_->head_bits[i]   << cmd_queue_->head_bits[i];
    cmd_state_->do_preloads[i] << cmd_decoder_->do_preloads[i];
    cmd_state_->do_computes[i] << cmd_decoder_->do_computes[i];
  }
  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    cmd_decoder_->tags_in_progress[i] << decoder_tags_in_progress_[i];
  }
  cmd_state_->do_config                       << cmd_decoder_->do_config;
  cmd_state_->raw_hazards_are_impossible      << cmd_decoder_->raw_hazards_are_impossible;
  cmd_state_->raw_hazard_pre                  << cmd_decoder_->raw_hazard_pre;
  cmd_state_->a_should_be_fed_into_transposer << cmd_decoder_->a_should_be_fed_into_transposer;
  cmd_state_->b_should_be_fed_into_transposer << cmd_decoder_->b_should_be_fed_into_transposer;
  cmd_state_->d_should_be_fed_into_transposer << cmd_decoder_->d_should_be_fed_into_transposer;
  cmd_state_->in_prop                         << cmd_decoder_->in_prop;
  // pass some HW build info to decoder
  cmd_decoder_->ex_read_from_acc << decoder_ex_read_from_acc_;    // const from SmeshConfig.hpp
  cmd_decoder_->ex_write_to_spad << decoder_ex_write_to_spad_;    // const from SmeshConfig.hpp
  // pass some config info processed by FSM to decoder
  cmd_decoder_->current_dataflow <= cmd_state_->current_dataflow; // dec gets FSM configs
  cmd_decoder_->a_transpose      <= cmd_state_->a_transpose;      // 
  cmd_decoder_->bd_transpose     <= cmd_state_->bd_transpose;     //
  // pass some other status signals to FSM
  cmd_state_->matmul_in_progress      << cmd_decoder_->matmul_in_progress;
  cmd_state_->pending_completed_valid << completion_->pending_completed_valid;
  // pass some status signals to completion block
  completion_->config_val          << cmd_state_->config_val;
  completion_->config_rs_tag_valid << cmd_state_->config_rs_tag_valid;
  completion_->config_rs_tag       << cmd_state_->config_rs_tag;
  // send out completed signals from ExCtrl
  completed_val << completion_->completed_val;
  completed_bits << completion_->completed_bits;
  // current-row address logic input
  cmd_rowaddr_->a_address_rs1     << cmd_decoder_->a_address_rs1;
  cmd_rowaddr_->b_address_rs2     << cmd_decoder_->b_address_rs2;
  cmd_rowaddr_->d_address_rs1     << cmd_decoder_->d_address_rs1;
  cmd_rowaddr_->a_addr_offset     << row_addr_a_addr_offset_;  // temp
  cmd_rowaddr_->b_fire_counter    << row_addr_b_fire_counter_; // temp
  cmd_rowaddr_->d_fire_counter    << row_addr_d_fire_counter_; // temp
  cmd_rowaddr_->block_size        << row_addr_block_size_;
  cmd_rowaddr_->ex_read_from_acc  << decoder_ex_read_from_acc_;
  cmd_rowaddr_->ws_no_transpose   << cmd_decoder_->ws_no_transpose;
  cmd_rowaddr_->a_rows            << cmd_decoder_->a_rows;
  cmd_rowaddr_->b_rows            << cmd_decoder_->b_rows;
  cmd_rowaddr_->start_inputting_a << cmd_state_->start_inputting_a;
  cmd_rowaddr_->start_inputting_b << cmd_state_->start_inputting_b;
  cmd_rowaddr_->start_inputting_d << cmd_state_->start_inputting_d;

  // row-padding logic input
  cmd_rowpad_->a_fire_counter << row_addr_a_addr_offset_;  // temp
  cmd_rowpad_->b_fire_counter << row_addr_b_fire_counter_; // temp
  cmd_rowpad_->d_fire_counter << row_addr_d_fire_counter_; // temp
  cmd_rowpad_->a_rows         << cmd_decoder_->a_rows;
  cmd_rowpad_->b_rows         << cmd_decoder_->b_rows;
  cmd_rowpad_->d_rows         << cmd_decoder_->d_rows;
  cmd_rowpad_->a_cols         << cmd_decoder_->a_cols;
  cmd_rowpad_->b_cols         << cmd_decoder_->b_cols;
  cmd_rowpad_->d_cols         << cmd_decoder_->d_cols;
  cmd_rowpad_->block_size     << row_addr_block_size_;

  UPDATE(updateReadPorts).writes(spad_read_req_val,
                                 spad_read_req_bits,
                                 spad_read_resp_rdy,
                                 accum_read_req_val,
                                 accum_read_req_bits,
                                 accum_read_resp_rdy);
  UPDATE(updateWritePorts).writes(spad_write_val,
                                  spad_write_bits,
                                  accum_write_val,
                                  accum_write_bits);
  UPDATE(updateDecoderInputs).writes(decoder_ex_read_from_acc_,
                                     decoder_ex_write_to_spad_,
                                     decoder_tags_in_progress_,
                                     row_addr_a_addr_offset_,
                                     row_addr_b_fire_counter_,
                                     row_addr_d_fire_counter_,
                                     row_addr_block_size_);
}

ExCtrl::~ExCtrl() {
  delete cmd_rowpad_;
  delete cmd_rowaddr_;
  delete cmd_state_;
  delete cmd_decoder_;
  delete completion_;
  delete cmd_queue_;
}

void ExCtrl::updateReadPorts() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_read_req_val[bank] = 0;
    spad_read_req_bits[bank] = SpadReadReq{};
    spad_read_resp_rdy[bank] = 0;
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_req_val[bank] = 0;
    accum_read_req_bits[bank] = AccumReadReq{};
    accum_read_resp_rdy[bank] = 0;
  }
}

void ExCtrl::updateWritePorts() {
  spad_write_val = 0;
  spad_write_bits = DmaReadResp{};

  accum_write_val = 0;
  accum_write_bits = DmaReadResp{};
}

void ExCtrl::updateDecoderInputs() {
  decoder_ex_read_from_acc_ = bit(kDefaultConfig.ex_read_from_acc);
  decoder_ex_write_to_spad_ = bit(kDefaultConfig.ex_write_to_spad);
  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    decoder_tags_in_progress_[i] = MesherTag{};
  }
  row_addr_a_addr_offset_   = 0;
  row_addr_b_fire_counter_  = 0;
  row_addr_d_fire_counter_  = 0;
  row_addr_block_size_      = static_cast<u32>(kDefaultConfig.dim);
}

void ExCtrl::reset() {
  decoder_ex_read_from_acc_.reset(bit(kDefaultConfig.ex_read_from_acc));
  decoder_ex_write_to_spad_.reset(bit(kDefaultConfig.ex_write_to_spad));
  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    decoder_tags_in_progress_[i].reset(MesherTag{});
  }
  row_addr_a_addr_offset_.reset(0);
  row_addr_b_fire_counter_.reset(0);
  row_addr_d_fire_counter_.reset(0);
  row_addr_block_size_.reset(static_cast<u32>(kDefaultConfig.dim));

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_read_req_val[bank].reset(0);
    spad_read_req_bits[bank].reset(SpadReadReq{});
    spad_read_resp_rdy[bank].reset(0);
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_req_val[bank].reset(0);
    accum_read_req_bits[bank].reset(AccumReadReq{});
    accum_read_resp_rdy[bank].reset(0);
  }

  spad_write_val.reset(0);
  spad_write_bits.reset(DmaReadResp{});

  accum_write_val.reset(0);
  accum_write_bits.reset(DmaReadResp{});
}

} // namespace smesh
