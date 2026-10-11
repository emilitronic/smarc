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

  // The internal router now controls when the oldest command leaves the queue.
  head_val_ << cmd_queue_->head_val;
  head_bits_ << cmd_queue_->head_bits;
  cmd_queue_->head_rdy << head_rdy_;

  // Decode the queue head; loop-bank storage will later accept these fields.
  cmd_dec_ = new LoopConvCmdDec("CmdDec");
  cmd_dec_->clk << clk;
  cmd_dec_->head_val << head_val_;
  cmd_dec_->head_bits << head_bits_;
  decoded_ << cmd_dec_->decoded;

  UPDATE(updateRouting)
      .reads(head_val_, head_bits_, decoded_, out_rdy)
      .writes(head_rdy_, out_val, out_bits, busy);
}

LoopConv::~LoopConv() {
  delete cmd_dec_;
  delete cmd_queue_;
}

// Send ordinary commands downstream; remove them only when the output accepts.
void LoopConv::updateRouting() {
  const auto decoded = *decoded_;
  const bool pass_through = head_val_ == 1 && decoded.do_config == 0 && decoded.do_launch == 0;
  out_val   = bit(pass_through);
  out_bits  = pass_through ? *head_bits_ : SmeshQueuedCmd{};
  head_rdy_ = bit(pass_through && out_rdy == 1);
  busy      = head_val_;
  // TODO: Consume loop commands through the decoder when its loop bank is free.
  // Once loop-bank state and workers exist, configured loops select worker output
  // and block ordinary pass-through until Original's retirement condition is met.
}

void LoopConv::reset() {
  head_rdy_.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshQueuedCmd{});
  busy.reset(0);
}

} // namespace smesh
