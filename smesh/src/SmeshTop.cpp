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
  cmd_queue_          = new SmeshCmdQueue("CmdQueue");
  unrolled_cmd_queue_ = new SmeshUnrolledCmdQueue("UnrolledCmdQueue");
  rs_                 = new SmeshRS("RS");
  ld_ctrl_            = new LdCtrl("LdCtrl");
  dma_reader_         = new DmaReader("DmaReader");
  mvin_scale_         = new MvinScale("MvinScale");
  pixel_repeater_     = new MvinPixelRepeater("MvinPixelRepeater");
  spad_               = new Spad("Spad");

  cmd_queue_->clk          << clk;
  unrolled_cmd_queue_->clk << clk;
  rs_->clk                 << clk;
  ld_ctrl_->clk            << clk;
  dma_reader_->clk         << clk;
  mvin_scale_->clk         << clk;
  pixel_repeater_->clk     << clk;
  spad_->clk               << clk;

  cmd_queue_->cmd_valid << cmd_valid;
  cmd_queue_->cmd_bits  << cmd_bits;
  cmd_ready             << cmd_queue_->cmd_ready;
  unrolled_cmd_queue_->cmd_in << cmd_queue_->cmd_out; // cmd_queue_ -> unrolled_cmd_queue_
  rs_->alloc_in << unrolled_cmd_queue_->cmd_out;      // unrolled_cmd_queue_ -> RS allocation
  ld_ctrl_->cmd_in << rs_->issue_ld;                  //                        RS load issue -> LdCtrl
  rs_->completed << ld_ctrl_->completed;              //                        RS completion <- LdCtrl
  dma_reader_->req_in << ld_ctrl_->dma_req;           //                                         LdCtrl -> DmaReader
  mvin_scale_->data_in << dma_reader_->resp_out;      //                                      MvinScale <- DmaReader
  pixel_repeater_->data_in << mvin_scale_->data_out;  //                 MvinPixelRepeater <- MvinScale 
  spad_->write_in << pixel_repeater_->data_out;       //        Spad <-  MvinPixelRepeater 
  ld_ctrl_->dma_resp << spad_->dma_resp;           // LdCtrl <- Spad completion
  rs_->setLoadIssuePortEnabled(true);

  UPDATE(update);
}

SmeshTop::~SmeshTop() {
  delete spad_;
  delete pixel_repeater_;
  delete mvin_scale_;
  delete dma_reader_;
  delete ld_ctrl_;
  delete rs_;
  delete unrolled_cmd_queue_;
  delete cmd_queue_;
}

void SmeshTop::update() {
  rs_->setLoadIssuePortEnabled(true);
}

void SmeshTop::reset() {
  rs_->setLoadIssuePortEnabled(true);
  trace("smesh_top: reset");
}

} // namespace smesh
