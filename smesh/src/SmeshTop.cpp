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
  cmd_queue_          = new SmeshCmdQueue("CmdQueue"); // buffer incoming commands here
  unrolled_cmd_queue_ = new SmeshUnrolledCmdQueue("UnrolledCmdQueue");
  rs_                 = new SmeshRS("RS");
  ld_ctrl_            = new LdCtrl("LdCtrl");
  dma_reader_         = new DmaReader("DmaReader");

  cmd_queue_->clk << clk;
  unrolled_cmd_queue_->clk << clk;
  rs_->clk << clk;
  ld_ctrl_->clk << clk;
  dma_reader_->clk << clk;

  cmd_queue_->cmd_in << cmd_in;           // SmeshTop -> cmd_queue_
  unrolled_cmd_queue_->cmd_in << cmd_queue_->cmd_out; // cmd_queue_ -> unrolled_cmd_queue_
  rs_->alloc_in << unrolled_cmd_queue_->cmd_out;                    // unrolled_cmd_queue_ -> RS allocation
  ld_ctrl_->cmd_in << rs_->issue_ld;                                                       // RS load issue -> LdCtrl
  rs_->completed << ld_ctrl_->completed;                                                   // RS completion <- LdCtrl
  dma_reader_->req_in << ld_ctrl_->dma_req;                                                                 // LdCtrl -> DmaReader
  ld_ctrl_->dma_resp.wireToZero();
  dma_reader_->mem_req.sendToBitBucket();
  dma_reader_->mem_resp.wireToZero();
  dma_reader_->resp_out.sendToBitBucket();
  rs_->setLoadIssuePortEnabled(true);

  UPDATE(update).reads(cmd_in);
}

SmeshTop::~SmeshTop() {
  delete dma_reader_;
  delete ld_ctrl_;
  delete rs_;
  delete unrolled_cmd_queue_;
  delete cmd_queue_;
}

void SmeshTop::update() {}

void SmeshTop::reset() {
  trace("smesh_top: reset");
}

} // namespace smesh
