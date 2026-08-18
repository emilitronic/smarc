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
  op_pack_     = new ExCtrlOperandPack("ExCtrlOperandPack");
  row_feed_    = new ExCtrlRowFeedState("ExCtrlRowFeedState");
  tag_select_  = new ExCtrlMeshTagSelect("ExCtrlMeshTagSelect");
  read_prio_   = new ExCtrlReadPriority("ExCtrlReadPriority");
  rd_req_      = new ExCtrlReadReqLogic("ExCtrlReadReqLogic");
  feed_signals_ = new ExCtrlFeedSignals("ExCtrlFeedSignals");
  mesh_cntl_pack_ = new ExCtrlMeshCntlPack("ExCtrlMeshCntlPack");
  mesh_cntl_queue_ = new ExCtrlMeshCntlQueue("ExCtrlMeshCntlQueue");

  cmd_queue_->clk   << clk;
  completion_->clk  << clk;
  cmd_decoder_->clk << clk;
  cmd_state_->clk   << clk;
  cmd_rowaddr_->clk << clk;
  cmd_rowpad_->clk  << clk;
  op_pack_->clk     << clk;
  row_feed_->clk    << clk;
  tag_select_->clk  << clk;
  read_prio_->clk   << clk;
  rd_req_->clk       << clk;
  feed_signals_->clk << clk;
  mesh_cntl_pack_->clk << clk;
  mesh_cntl_queue_->clk << clk;
  
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
    tag_select_->head_val[i]   << cmd_queue_->head_val[i];
    tag_select_->head_bits[i]  << cmd_queue_->head_bits[i];
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
  cmd_rowaddr_->a_addr_offset     << row_feed_->a_addr_offset;
  cmd_rowaddr_->b_fire_counter    << row_feed_->b_fire_counter;
  cmd_rowaddr_->d_fire_counter    << row_feed_->d_fire_counter;
  cmd_rowaddr_->block_size        << row_addr_block_size_;
  cmd_rowaddr_->ex_read_from_acc  << decoder_ex_read_from_acc_;
  cmd_rowaddr_->ws_no_transpose   << cmd_decoder_->ws_no_transpose;
  cmd_rowaddr_->a_rows            << cmd_decoder_->a_rows;
  cmd_rowaddr_->b_rows            << cmd_decoder_->b_rows;
  cmd_rowaddr_->start_inputting_a << cmd_state_->start_inputting_a;
  cmd_rowaddr_->start_inputting_b << cmd_state_->start_inputting_b;
  cmd_rowaddr_->start_inputting_d << cmd_state_->start_inputting_d;

  // row-padding logic input
  cmd_rowpad_->a_fire_counter << row_feed_->a_fire_counter;
  cmd_rowpad_->b_fire_counter << row_feed_->b_fire_counter;
  cmd_rowpad_->d_fire_counter << row_feed_->d_fire_counter;
  cmd_rowpad_->a_rows         << cmd_decoder_->a_rows;
  cmd_rowpad_->b_rows         << cmd_decoder_->b_rows;
  cmd_rowpad_->d_rows         << cmd_decoder_->d_rows;
  cmd_rowpad_->a_cols         << cmd_decoder_->a_cols;
  cmd_rowpad_->b_cols         << cmd_decoder_->b_cols;
  cmd_rowpad_->d_cols         << cmd_decoder_->d_cols;
  cmd_rowpad_->block_size     << row_addr_block_size_;

  // mesh completion-tag selection for future mesh-control queue enqueue path
  tag_select_->preload_cmd_place      << cmd_decoder_->preload_cmd_place;
  tag_select_->performing_single_mul  << tag_select_performing_single_mul_; // temp until FSM exposes this mode
  tag_select_->c_address_rs2          << cmd_decoder_->c_address_rs2;

  // operand packaging for A/B/D read-priority logic
  op_pack_->a_address          << cmd_rowaddr_->a_address;
  op_pack_->b_address          << cmd_rowaddr_->b_address;
  op_pack_->d_address          << cmd_rowaddr_->d_address;
  op_pack_->a_address_rs1      << cmd_decoder_->a_address_rs1;
  op_pack_->b_address_rs2      << cmd_decoder_->b_address_rs2;
  op_pack_->d_address_rs1      << cmd_decoder_->d_address_rs1;
  op_pack_->start_inputting_a  << cmd_state_->start_inputting_a;
  op_pack_->start_inputting_b  << cmd_state_->start_inputting_b;
  op_pack_->start_inputting_d  << cmd_state_->start_inputting_d;
  op_pack_->a_fire_counter     << row_feed_->a_fire_counter;
  op_pack_->b_fire_counter     << row_feed_->b_fire_counter;
  op_pack_->d_fire_counter     << row_feed_->d_fire_counter;
  op_pack_->a_fire_started     << row_feed_->a_fire_started;
  op_pack_->b_fire_started     << row_feed_->b_fire_started;
  op_pack_->d_fire_started     << row_feed_->d_fire_started;

  // A/B/D read-priority gating; im2col is explicitly idle in this model stage.
  read_prio_->a_operand    << op_pack_->a_operand;
  read_prio_->b_operand    << op_pack_->b_operand;
  read_prio_->d_operand    << op_pack_->d_operand;
  read_prio_->total_rows   << cmd_rowaddr_->total_rows;
  read_prio_->im2col_wire  << im2col_wire_;
  read_prio_->im2col_en    << im2col_en_;

  // A/B/D operand-read requests toward the banked local memories.
  rd_req_->start_inputting_a << cmd_state_->start_inputting_a;
  rd_req_->start_inputting_b << cmd_state_->start_inputting_b;
  rd_req_->start_inputting_d << cmd_state_->start_inputting_d;
  rd_req_->a_address         << cmd_rowaddr_->a_address;
  rd_req_->b_address         << cmd_rowaddr_->b_address;
  rd_req_->d_address         << cmd_rowaddr_->d_address;
  rd_req_->a_valid           << read_prio_->a_valid;
  rd_req_->b_valid           << read_prio_->b_valid;
  rd_req_->d_valid           << read_prio_->d_valid;
  rd_req_->a_row_is_not_all_zeros << cmd_rowpad_->a_row_is_not_all_zeros;
  rd_req_->b_row_is_not_all_zeros << cmd_rowpad_->b_row_is_not_all_zeros;
  rd_req_->d_row_is_not_all_zeros << cmd_rowpad_->d_row_is_not_all_zeros;
  rd_req_->multiply_garbage       << cmd_decoder_->multiply_garbage;
  rd_req_->accumulate_zeros       << cmd_decoder_->accumulate_zeros;
  rd_req_->preload_zeros          << cmd_decoder_->preload_zeros;
  rd_req_->a_read_from_acc        << cmd_rowaddr_->a_read_from_acc;
  rd_req_->b_read_from_acc        << cmd_rowaddr_->b_read_from_acc;
  rd_req_->d_read_from_acc        << cmd_rowaddr_->d_read_from_acc;
  rd_req_->dataAbank              << cmd_rowaddr_->dataAbank;
  rd_req_->dataBbank              << cmd_rowaddr_->dataBbank;
  rd_req_->dataDbank              << cmd_rowaddr_->dataDbank;
  rd_req_->dataABankAcc           << cmd_rowaddr_->dataABankAcc;
  rd_req_->dataBBankAcc           << cmd_rowaddr_->dataBBankAcc;
  rd_req_->dataDBankAcc           << cmd_rowaddr_->dataDBankAcc;
  rd_req_->cntl_rdy               << cntl_rdy_;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    rd_req_->spad_read_req_rdy[bank] << spad_read_req_rdy[bank];
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    rd_req_->accum_read_req_rdy[bank] << accum_read_req_rdy[bank];
  }

  // Derived row-feed handshake signals. These will drive row-feed state and MQ
  // packaging once those paths are connected.
  feed_signals_->start_inputting_a << cmd_state_->start_inputting_a;
  feed_signals_->start_inputting_b << cmd_state_->start_inputting_b;
  feed_signals_->start_inputting_d << cmd_state_->start_inputting_d;
  feed_signals_->a_valid           << read_prio_->a_valid;
  feed_signals_->b_valid           << read_prio_->b_valid;
  feed_signals_->d_valid           << read_prio_->d_valid;
  feed_signals_->a_ready           << rd_req_->a_ready;
  feed_signals_->b_ready           << rd_req_->b_ready;
  feed_signals_->d_ready           << rd_req_->d_ready;

  row_feed_->firing << feed_signals_->firing;
  row_feed_->a_fire << feed_signals_->a_fire;
  row_feed_->b_fire << feed_signals_->b_fire;
  row_feed_->d_fire << feed_signals_->d_fire;
  row_feed_->total_rows << cmd_rowaddr_->total_rows;
  row_feed_->a_addr_stride << cmd_state_->a_addr_stride;
  row_feed_->cntl_rdy << cntl_rdy_;

  // Mesh-control packet packaging. Its output will feed MQ once MQ is installed
  // inside ExCtrl.
  mesh_cntl_pack_->perform_mul_pre        << mesh_cntl_pack_perform_mul_pre_;
  mesh_cntl_pack_->perform_single_mul     << tag_select_performing_single_mul_;
  mesh_cntl_pack_->perform_single_preload << cmd_state_->performing_single_preload;
  mesh_cntl_pack_->a_bank                 << cmd_rowaddr_->dataAbank;
  mesh_cntl_pack_->b_bank                 << cmd_rowaddr_->dataBbank;
  mesh_cntl_pack_->d_bank                 << cmd_rowaddr_->dataDbank;
  mesh_cntl_pack_->a_bank_acc             << cmd_rowaddr_->dataABankAcc;
  mesh_cntl_pack_->b_bank_acc             << cmd_rowaddr_->dataBBankAcc;
  mesh_cntl_pack_->d_bank_acc             << cmd_rowaddr_->dataDBankAcc;
  mesh_cntl_pack_->a_read_from_acc        << cmd_rowaddr_->a_read_from_acc;
  mesh_cntl_pack_->b_read_from_acc        << cmd_rowaddr_->b_read_from_acc;
  mesh_cntl_pack_->d_read_from_acc        << cmd_rowaddr_->d_read_from_acc;
  mesh_cntl_pack_->a_garbage              << cmd_rowaddr_->a_garbage;
  mesh_cntl_pack_->b_garbage              << cmd_rowaddr_->b_garbage;
  mesh_cntl_pack_->d_garbage              << cmd_rowaddr_->d_garbage;
  mesh_cntl_pack_->accumulate_zeros       << cmd_decoder_->accumulate_zeros;
  mesh_cntl_pack_->preload_zeros          << cmd_decoder_->preload_zeros;
  mesh_cntl_pack_->a_fire                 << feed_signals_->a_fire;
  mesh_cntl_pack_->b_fire                 << feed_signals_->b_fire;
  mesh_cntl_pack_->d_fire                 << feed_signals_->d_fire;
  mesh_cntl_pack_->a_unpadded_cols        << cmd_rowpad_->a_unpadded_cols;
  mesh_cntl_pack_->b_unpadded_cols        << cmd_rowpad_->b_unpadded_cols;
  mesh_cntl_pack_->d_unpadded_cols        << cmd_rowpad_->d_unpadded_cols;
  mesh_cntl_pack_->c_addr                 << cmd_decoder_->c_address_rs2;
  mesh_cntl_pack_->c_rows                 << cmd_decoder_->c_rows;
  mesh_cntl_pack_->c_cols                 << cmd_decoder_->c_cols;
  mesh_cntl_pack_->a_transpose            << cmd_state_->a_transpose;
  mesh_cntl_pack_->bd_transpose           << cmd_state_->bd_transpose;
  mesh_cntl_pack_->total_rows             << cmd_rowaddr_->total_rows;
  mesh_cntl_pack_->rs_tag_valid           << tag_select_->mesh_rs_tag_valid;
  mesh_cntl_pack_->rs_tag                 << tag_select_->mesh_rs_tag;
  mesh_cntl_pack_->dataflow               << cmd_state_->current_dataflow;
  mesh_cntl_pack_->prop                   << cmd_state_->prop;
  mesh_cntl_pack_->shift                  << cmd_state_->shift;
  mesh_cntl_pack_->im2colling             << im2colling_;
  mesh_cntl_pack_->first                  << row_feed_->first;

  // MQ: one control packet per active row-feed cycle. Dequeue is held idle
  // until the Mesher/deq-control side is installed in ExCtrl.
  mesh_cntl_queue_->enq_val            << cmd_state_->computing;
  mesh_cntl_queue_->enq_bits           << mesh_cntl_pack_->enq_bits;
  mesh_cntl_queue_->mesh_cntl_deq_rdy  << mesh_cntl_deq_rdy_;

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
                                     mesh_cntl_pack_perform_mul_pre_,
                                     tag_select_performing_single_mul_,
                                     im2col_wire_,
                                     im2col_en_)
                             .writes(im2colling_,
                                     mesh_cntl_deq_rdy_,
                                     cntl_rdy_,
                                     decoder_tags_in_progress_,
                                     row_addr_block_size_);
}

ExCtrl::~ExCtrl() {
  delete mesh_cntl_queue_;
  delete mesh_cntl_pack_;
  delete feed_signals_;
  delete rd_req_;
  delete read_prio_;
  delete tag_select_;
  delete op_pack_;
  delete cmd_rowpad_;
  delete cmd_rowaddr_;
  delete row_feed_;
  delete cmd_state_;
  delete cmd_decoder_;
  delete completion_;
  delete cmd_queue_;
}

void ExCtrl::updateReadPorts() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    SpadReadReq req{};
    req.laddr = *rd_req_->spad_read_req_addr[bank];
    req.from_dma = rd_req_->spad_read_req_from_dma[bank];
    spad_read_req_val[bank] = rd_req_->spad_read_req_val[bank];
    spad_read_req_bits[bank] = req;
    spad_read_resp_rdy[bank] = 0;
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    AccumReadReq req{};
    req.laddr = *rd_req_->accum_read_req_addr[bank];
    req.scale = rd_req_->accum_read_req_scale[bank];
    req.full = rd_req_->accum_read_req_full[bank];
    req.act = rd_req_->accum_read_req_act[bank];
    req.igelu_qb = rd_req_->accum_read_req_igelu_qb[bank];
    req.igelu_qc = rd_req_->accum_read_req_igelu_qc[bank];
    req.iexp_qln2 = rd_req_->accum_read_req_iexp_qln2[bank];
    req.iexp_qln2_inv = rd_req_->accum_read_req_iexp_qln2_inv[bank];
    req.from_dma = rd_req_->accum_read_req_from_dma[bank];
    accum_read_req_val[bank] = rd_req_->accum_read_req_val[bank];
    accum_read_req_bits[bank] = req;
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
  mesh_cntl_pack_perform_mul_pre_ = 0;
  tag_select_performing_single_mul_ = 0;
  im2col_wire_ = 0;
  im2col_en_ = 0;
  im2colling_ = 0;
  mesh_cntl_deq_rdy_ = 0; // TODO: connect to ExCtrlMeshCntlDeqCtrl once installed in ExCtrl.
  cntl_rdy_ = mesh_cntl_queue_->enq_rdy;
  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    decoder_tags_in_progress_[i] = MesherTag{};
  }
  row_addr_block_size_      = static_cast<u32>(kDefaultConfig.dim);
}

void ExCtrl::reset() {
  decoder_ex_read_from_acc_.reset(bit(kDefaultConfig.ex_read_from_acc));
  decoder_ex_write_to_spad_.reset(bit(kDefaultConfig.ex_write_to_spad));
  mesh_cntl_pack_perform_mul_pre_.reset(0);
  tag_select_performing_single_mul_.reset(0);
  im2col_wire_.reset(0);
  im2col_en_.reset(0);
  im2colling_.reset(0);
  mesh_cntl_deq_rdy_.reset(0);
  cntl_rdy_.reset(1);
  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    decoder_tags_in_progress_[i].reset(MesherTag{});
  }
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
