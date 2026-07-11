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
  st_ctrl_              = new StCtrl("StCtrl");
  write_dispatch_queue_ = new DmaWriteDispatchQueue("DmaWriteDispatchQueue");
  write_norm_queue_     = new DmaWriteNormQueue("DmaWriteNormQueue");
  write_scale_queue_    = new DmaWriteScaleQueue("DmaWriteScaleQueue");
  write_issue_queue_    = new DmaWriteIssueQueue("DmaWriteIssueQueue");
  dma_reader_           = new DmaReader("DmaReader");
  mvin_scale_           = new MvinScale("MvinScale");
  pixel_repeater_       = new MvinPixelRepeater("MvinPixelRepeater");
  local_router_         = new MvinLocalRouter("MvinLocalRouter");
  spad_                 = new Spad("Spad");
  accum_                = new Accum("Accum");
  completion_mux_       = new DmaReadCompletionMux("DmaReadCompletionMux");

  cmd_queue_->clk            << clk;
  unrolled_cmd_queue_->clk   << clk;
  rs_->clk                   << clk;
  ld_ctrl_->clk              << clk;
  read_issue_queue_->clk     << clk;
  st_ctrl_->clk              << clk;
  write_dispatch_queue_->clk << clk;
  write_norm_queue_->clk     << clk;
  write_scale_queue_->clk    << clk;
  write_issue_queue_->clk    << clk;
  dma_reader_->clk           << clk;
  mvin_scale_->clk           << clk;
  pixel_repeater_->clk       << clk;
  local_router_->clk         << clk;
  spad_->clk                 << clk;
  accum_->clk                << clk;
  completion_mux_->clk       << clk;

  cmd_queue_->cmd_valid << cmd_valid;
  cmd_queue_->cmd_bits  << cmd_bits;
  cmd_ready             << cmd_queue_->cmd_ready;
  unrolled_cmd_queue_->cmd_in << cmd_queue_->cmd_out;  // cmd_queue_ -> unrolled_cmd_queue_
  rs_->alloc_in << unrolled_cmd_queue_->cmd_out;       //               unrolled_cmd_queue_ -> RS allocation
  ld_ctrl_->cmd_in << rs_->issue_ld;                   //                                      RS load issue -> LdCtrl
  rs_->completed << ld_ctrl_->completed;               //                                      RS completion <- LdCtrl
  read_issue_queue_->req_in << ld_ctrl_->dma_req;      //                                                       LdCtrl -> DmaReadIssueQueue
  dma_reader_->req_in << read_issue_queue_->req_out;   //                                           DmaReadIssueQueue -> DmaReader
  st_ctrl_->cmd_in << rs_->issue_st;                   //                                      RS store issue -> StCtrl
  write_dispatch_queue_->req_in << st_ctrl_->dma_req;  //                                                       StCtrl -> DmaWriteDispatchQueue
  write_norm_queue_->req_in << write_dispatch_queue_->req_out;
  write_scale_queue_->req_in << write_norm_queue_->req_out;
  write_issue_queue_->req_in << write_scale_queue_->req_out;
  write_issue_queue_->req_out.sendToBitBucket();       // later: write issue feeds DmaWriter
  st_ctrl_->completed.sendToBitBucket();               // later: store completions will join RS completion arbitration
  st_ctrl_->completed.wireToZero();
  mvin_scale_->data_in << dma_reader_->resp_out;       //                                                    MvinScale <- DmaReader
  pixel_repeater_->data_in << mvin_scale_->data_out;   //                               MvinPixelRepeater <- MvinScale 
  local_router_->data_in << pixel_repeater_->data_out; //            MvinLocalRouter <- MvinPixelRepeater
  spad_->write_in << local_router_->spad_out;          //    Spad <- MvinLocalRouter
  accum_->write_in << local_router_->accum_out;        //   Accum <- MvinLocalRouter
  spad_->read_req.wireToZero();                        // later: write_dispatch_queue_ -> local-memory read path
  spad_->read_resp.sendToBitBucket();
  accum_->read_req.wireToZero();                       // later: write_dispatch_queue_ -> local-memory read path
  accum_->read_resp.sendToBitBucket();
  completion_mux_->spad_in << spad_->dma_resp;         //             DmaReadCompletionMux <- Spad
  completion_mux_->accum_in << accum_->dma_resp;       //             DmaReadCompletionMux <- Accum
  ld_ctrl_->dma_resp << completion_mux_->dma_resp;     //   LdCtrl <- DmaReadCompletionMux
  rs_->setLoadIssuePortEnabled(true);
  rs_->setStoreIssuePortEnabled(true);

  UPDATE(update);
}

SmeshTop::~SmeshTop() {
  delete completion_mux_;
  delete accum_;
  delete spad_;
  delete local_router_;
  delete pixel_repeater_;
  delete mvin_scale_;
  delete dma_reader_;
  delete write_issue_queue_;
  delete write_scale_queue_;
  delete write_norm_queue_;
  delete write_dispatch_queue_;
  delete st_ctrl_;
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
