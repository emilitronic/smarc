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
  cmd_queue_ = new SmeshCmdQueue("CmdQueue"); // buffer incoming commands here

  cmd_queue_->clk << clk;
  cmd_queue_->cmd_in << cmd_in; // SmeshTop -> cmd_queue_
  cmd_queue_->cmd_out.sendToBitBucket();

  UPDATE(update).reads(cmd_in);
}

SmeshTop::~SmeshTop() {
  delete cmd_queue_;
}

void SmeshTop::update() {}

void SmeshTop::reset() {
  trace("smesh_top: reset");
}

} // namespace smesh
