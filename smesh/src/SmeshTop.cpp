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

  cmd_queue_->clk << clk;
  unrolled_cmd_queue_->clk << clk;
  rs_->clk << clk;

  cmd_queue_->cmd_in << cmd_in;                       // SmeshTop -> cmd_queue_
  unrolled_cmd_queue_->cmd_in << cmd_queue_->cmd_out; // cmd_queue_ -> unrolled_cmd_queue_
  rs_->alloc_in << unrolled_cmd_queue_->cmd_out;      // unrolled_cmd_queue_ -> RS allocation
  rs_->issue_ld.sendToBitBucket();
  rs_->completed.wireToZero();

  UPDATE(update).reads(cmd_in);
}

SmeshTop::~SmeshTop() {
  delete rs_;
  delete unrolled_cmd_queue_;
  delete cmd_queue_;
}

void SmeshTop::update() {}

void SmeshTop::reset() {
  trace("smesh_top: reset");
}

} // namespace smesh
