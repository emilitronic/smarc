// **********************************************************************
// smesh/src/loop_conv/LoopConv.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConv.hpp"
#include "LoopConvCmdQueue.hpp"

namespace smesh {

LoopConv::LoopConv(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_ = new LoopConvCmdQueue("CmdQueue");
  cmd_queue_->clk << clk;

  // Buffer incoming commands before any LoopConv interpretation.
  cmd_queue_->cmd_val << in_val;
  cmd_queue_->cmd_bits << in_bits;
  in_rdy << cmd_queue_->cmd_rdy;

  // Expose the queued command until the internal decoder/router is built.
  head_val << cmd_queue_->head_val;
  head_bits << cmd_queue_->head_bits;
  cmd_queue_->head_rdy << head_rdy;
  busy << cmd_queue_->head_val;
}

LoopConv::~LoopConv() {
  delete cmd_queue_;
}

} // namespace smesh
