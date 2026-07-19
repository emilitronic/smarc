// **********************************************************************
// smesh/src/SmeshTop.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Top-level smesh composition point.
*/

#include "SmeshTop.hpp"

namespace smesh {

SmeshTop::SmeshTop(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_            = new SmeshCmdQueue("CmdQueue");
  unrolled_cmd_queue_   = new SmeshUnrolledCmdQueue("UnrolledCmdQueue");
  rs_                   = new SmeshRS("RS");
  ld_ctrl_              = new LdCtrl("LdCtrl");
  read_issue_queue_     = new DmaReadIssueQueue("DmaReadIssueQueue");
  ex_ctrl_              = new ExCtrl("ExCtrl");
  st_ctrl_              = new StCtrl("StCtrl");
  write_dispatch_queue_ = new DmaWriteDispatchQueue("DmaWriteDispatchQueue");
  st_read_ctrl_         = new StReadCtrl("StReadCtrl");
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad_[bank] = new ArbReadSpad("ArbReadSpad");
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    arb_read_accum_[bank] = new ArbReadAccum("ArbReadAccum");
  }
  write_norm_queue_     = new DmaWriteNormQueue("DmaWriteNormQueue");
  st_norm_ctrl_         = new StNormCtrl("StNormCtrl");
  normalizer_           = new Normalizer("Normalizer");
  acc_scale_unit_       = new AccScaleUnit("AccScaleUnit");
  st_scale_ctrl_        = new StScaleCtrl("StScaleCtrl");
  write_scale_queue_    = new DmaWriteScaleQueue("DmaWriteScaleQueue");
  write_issue_queue_    = new DmaWriteIssueQueue("DmaWriteIssueQueue");
  st_issue_ctrl_        = new StIssueCtrl("StIssueCtrl");
  st_issue_mux_         = new StIssueMux("StIssueMux");
  dma_writer_           = new DmaWriter("DmaWriter");
  spad_writer_          = new SpadWriter("SpadWriter");
  dma_reader_           = new DmaReader("DmaReader");
  mvin_scale_           = new MvinScale("MvinScale");
  pixel_repeater_       = new MvinPixelRepeater("MvinPixelRepeater");
  local_router_         = new MvinLocalRouter("MvinLocalRouter");
  spad_                 = new Spad("Spad");
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_dma_read_pipe_[bank] = new SpadDmaReadPipe("SpadDmaReadPipe");
  }
  accum_                = new Accum("Accum");
  completion_mux_       = new DmaReadCompletionMux("DmaReadCompletionMux");

  cmd_queue_->clk            << clk;
  unrolled_cmd_queue_->clk   << clk;
  rs_->clk                   << clk;
  ld_ctrl_->clk              << clk;
  read_issue_queue_->clk     << clk;
  ex_ctrl_->clk              << clk;
  st_ctrl_->clk              << clk;
  write_dispatch_queue_->clk << clk;
  st_read_ctrl_->clk         << clk;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad_[bank]->clk << clk;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    arb_read_accum_[bank]->clk << clk;
  }
  write_norm_queue_->clk     << clk;
  st_norm_ctrl_->clk         << clk;
  st_norm_ctrl_->bank_index  << u32(0); // we don't have banks structures defined yet, set to 0 for now
  normalizer_->clk           << clk;
  acc_scale_unit_->clk       << clk;
  st_scale_ctrl_->clk        << clk;
  write_scale_queue_->clk    << clk;
  write_issue_queue_->clk    << clk;
  st_issue_ctrl_->clk        << clk;
  st_issue_mux_->clk         << clk;
  dma_writer_->clk           << clk;
  spad_writer_->clk          << clk;
  dma_reader_->clk           << clk;
  mvin_scale_->clk           << clk;
  pixel_repeater_->clk       << clk;
  local_router_->clk         << clk;
  spad_->clk                 << clk;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_dma_read_pipe_[bank]->clk << clk;
  }
  accum_->clk                << clk;
  completion_mux_->clk       << clk;

  cmd_queue_->cmd_valid << cmd_valid;
  cmd_queue_->cmd_bits  << cmd_bits;
  cmd_ready             << cmd_queue_->cmd_ready;
  unrolled_cmd_queue_->cmd_in << cmd_queue_->cmd_out;  
  rs_->alloc_in    << unrolled_cmd_queue_->cmd_out;       
  ld_ctrl_->cmd_in << rs_->issue_ld;                   
  rs_->completed   << ld_ctrl_->completed;               
  read_issue_queue_->req_in << ld_ctrl_->dma_req;      
  dma_reader_->req_in       << read_issue_queue_->req_out;   
  ex_ctrl_->cmd_in << rs_->issue_ex;
  ex_ctrl_->completed.sendToBitBucket();
  ex_ctrl_->completed.wireToZero();
  st_ctrl_->cmd_in << rs_->issue_st;                   
  write_dispatch_queue_->req_in << st_ctrl_->dma_req;  
  st_read_ctrl_->dispatch_val       << write_dispatch_queue_->deq_val;
  st_read_ctrl_->dispatch_bits      << write_dispatch_queue_->deq_bits;
  st_read_ctrl_->norm_rdy           << write_norm_queue_->enq_rdy;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    st_read_ctrl_->spad_read_req_rdy[bank] << arb_read_spad_[bank]->dmawrite_rdy;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    st_read_ctrl_->accum_read_req_rdy[bank] << arb_read_accum_[bank]->dmawrite_rdy;
  }
  write_dispatch_queue_->deq_rdy << st_read_ctrl_->read_req_fire;
  write_norm_queue_->enq_val   << st_read_ctrl_->read_req_fire;
  write_norm_queue_->enq_bits  << write_dispatch_queue_->deq_bits;
  st_norm_ctrl_->norm_deq_val  << write_norm_queue_->deq_val;
  st_norm_ctrl_->norm_deq_bits << write_norm_queue_->deq_bits;
  st_norm_ctrl_->normalizer_cmd_rdy << normalizer_->req_rdy;
  normalizer_->req_val  << st_norm_ctrl_->normalizer_cmd_val;
  normalizer_->req_bits << st_norm_ctrl_->normalizer_req_bits;
  st_scale_ctrl_->normalizer_resp_val  << normalizer_->resp_val;
  st_scale_ctrl_->normalizer_resp_bits << normalizer_->resp_bits;
  normalizer_->resp_rdy                << st_scale_ctrl_->normalizer_resp_rdy;
  st_norm_ctrl_->scale_enq_rdy << write_scale_queue_->enq_rdy;
  write_scale_queue_->enq_val  << st_norm_ctrl_->scale_enq_val;
  write_scale_queue_->enq_bits << write_norm_queue_->deq_bits;
  write_norm_queue_->deq_rdy   << st_norm_ctrl_->norm_deq_rdy;
  st_scale_ctrl_->scale_deq_val << write_scale_queue_->deq_val;
  st_scale_ctrl_->scale_deq_bits << write_scale_queue_->deq_bits;
  write_scale_queue_->deq_rdy   << st_scale_ctrl_->scale_deq_rdy;
  st_scale_ctrl_->acc_scale_req_rdy << acc_scale_unit_->req_rdy;
  acc_scale_unit_->req_val  << st_scale_ctrl_->acc_scale_req_val;
  acc_scale_unit_->req_bits << st_scale_ctrl_->acc_scale_req_bits;
  st_scale_ctrl_->issue_enq_rdy << write_issue_queue_->enq_rdy;
  write_issue_queue_->enq_val  << st_scale_ctrl_->issue_enq_val;
  write_issue_queue_->enq_bits << write_scale_queue_->deq_bits;
  st_issue_ctrl_->issue_deq_val  << write_issue_queue_->deq_val;
  st_issue_ctrl_->issue_deq_bits << write_issue_queue_->deq_bits;
  st_issue_ctrl_->dma_writer_req_rdy  << dma_writer_->req_rdy;
  st_issue_ctrl_->spad_writer_req_rdy << spad_writer_->req_rdy;
  st_issue_ctrl_->spad_data_val       << spad_dma_read_pipe_[0]->out_val;
  st_issue_ctrl_->spad_data_bits      << spad_dma_read_pipe_[0]->out_bits;
  st_issue_ctrl_->acc_data_val        << acc_scale_unit_->out_val;
  st_issue_ctrl_->acc_data_bits       << acc_scale_unit_->out_bits;
  st_issue_mux_->issue_bits       << write_issue_queue_->deq_bits;
  st_issue_mux_->spad_data_bits   << spad_dma_read_pipe_[0]->out_bits;
  st_issue_mux_->acc_data_bits    << acc_scale_unit_->out_bits;
  st_issue_mux_->data_source_sel  << st_issue_ctrl_->data_source_sel;
  st_issue_mux_->final_data_sel   << st_issue_ctrl_->final_data_sel;
  st_issue_mux_->write_data_is_all_zeros  << st_issue_ctrl_->write_data_is_all_zeros;
  st_issue_mux_->write_data_is_full_width << st_issue_ctrl_->write_data_is_full_width;
  dma_writer_->req_val  << st_issue_ctrl_->dma_writer_req_val;
  dma_writer_->req_bits << st_issue_mux_->writer_req_bits;
  spad_writer_->req_val  << st_issue_ctrl_->spad_writer_req_val;
  spad_writer_->req_bits << st_issue_mux_->writer_req_bits;
  dma_writer_->mem_req.sendToBitBucket();              // later: connect to store-side external memory boundary
  spad_writer_->spad_write_out.sendToBitBucket();      // later: connect to store-spad destination path
  write_issue_queue_->deq_rdy << st_issue_ctrl_->issue_deq_rdy;
  st_ctrl_->completed.sendToBitBucket();               // later: store completions will join RS completion arbitration
  st_ctrl_->completed.wireToZero();
  mvin_scale_->data_in     << dma_reader_->resp_out;       
  pixel_repeater_->data_in << mvin_scale_->data_out;    
  local_router_->data_in   << pixel_repeater_->data_out; 
  spad_->write_in          << local_router_->spad_out; 
  accum_->write_in         << local_router_->accum_out; 
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad_[bank]->exread_val    << ex_ctrl_->spad_read_req_val[bank];
    arb_read_spad_[bank]->exread_bits   << ex_ctrl_->spad_read_req_bits[bank];
    ex_ctrl_->spad_read_req_rdy[bank]   << arb_read_spad_[bank]->exread_rdy;
    arb_read_spad_[bank]->dmawrite_val  << st_read_ctrl_->dmawrite_spad[bank];
    arb_read_spad_[bank]->dmawrite_bits << st_read_ctrl_->spad_req_bits[bank];
    arb_read_spad_[bank]->read_req_rdy  << spad_->read_req_rdy_bnk[bank];
    spad_->read_req_val_bnk[bank]    << arb_read_spad_[bank]->read_req_val;
    spad_->read_req_bits_bnk[bank]   << arb_read_spad_[bank]->read_req_bits;
  }
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_dma_read_pipe_[bank]->resp_val  << spad_->read_resp_val_bnk[bank];
    spad_dma_read_pipe_[bank]->resp_bits << spad_->read_resp_bits_bnk[bank];
    spad_->read_resp_rdy_bnk[bank]       << spad_dma_read_pipe_[bank]->resp_rdy;
  }
  spad_dma_read_pipe_[0]->out_rdy << st_issue_ctrl_->spad_data_rdy;
  for (std::size_t bank = 1; bank < kSpBanks; ++bank) {
    spad_dma_read_pipe_[bank]->out_rdy << bit(0);
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    arb_read_accum_[bank]->exread_val    << ex_ctrl_->accum_read_req_val[bank];
    arb_read_accum_[bank]->exread_bits   << ex_ctrl_->accum_read_req_bits[bank];
    ex_ctrl_->accum_read_req_rdy[bank]   << arb_read_accum_[bank]->exread_rdy;
    arb_read_accum_[bank]->dmawrite_val  << st_read_ctrl_->dmawrite_accum[bank];
    arb_read_accum_[bank]->dmawrite_bits << st_read_ctrl_->accum_req_bits[bank];
    arb_read_accum_[bank]->read_req_rdy  << accum_->read_req_rdy_bnk[bank];
    accum_->read_req_val_bnk[bank]    << arb_read_accum_[bank]->read_req_val;
    accum_->read_req_bits_bnk[bank]   << arb_read_accum_[bank]->read_req_bits;
  }
  acc_scale_unit_->out_rdy << st_issue_ctrl_->acc_data_rdy;
  st_norm_ctrl_->accum_read_resp_val  << accum_->read_resp_val;
  st_norm_ctrl_->accum_read_resp_bits << accum_->read_resp_bits;
  accum_->read_resp_rdy               << st_norm_ctrl_->accum_read_resp_rdy;
  completion_mux_->spad_in  << spad_->dma_resp;         
  completion_mux_->accum_in << accum_->dma_resp;       
  ld_ctrl_->dma_resp        << completion_mux_->dma_resp;     
  rs_->setLoadIssuePortEnabled(true);
  rs_->setStoreIssuePortEnabled(true);

  UPDATE(update);
}

SmeshTop::~SmeshTop() {
  delete completion_mux_;
  delete accum_;
  for (auto* pipe : spad_dma_read_pipe_) {
    delete pipe;
  }
  delete spad_;
  delete local_router_;
  delete pixel_repeater_;
  delete mvin_scale_;
  delete dma_reader_;
  delete spad_writer_;
  delete dma_writer_;
  delete st_issue_mux_;
  delete write_issue_queue_;
  delete st_issue_ctrl_;
  delete write_scale_queue_;
  delete st_scale_ctrl_;
  delete acc_scale_unit_;
  delete normalizer_;
  delete st_norm_ctrl_;
  delete write_norm_queue_;
  for (auto* arb : arb_read_accum_) {
    delete arb;
  }
  for (auto* arb : arb_read_spad_) {
    delete arb;
  }
  delete st_read_ctrl_;
  delete write_dispatch_queue_;
  delete st_ctrl_;
  delete ex_ctrl_;
  delete read_issue_queue_;
  delete ld_ctrl_;
  delete rs_;
  delete unrolled_cmd_queue_;
  delete cmd_queue_;
}

void SmeshTop::update() {
  rs_->setLoadIssuePortEnabled(true);
  rs_->setStoreIssuePortEnabled(true);
}

void SmeshTop::reset() {
  rs_->setLoadIssuePortEnabled(true);
  rs_->setStoreIssuePortEnabled(true);
  trace("smesh_top: reset");
}

} // namespace smesh
