// **********************************************************************
// smesh/src/loop_conv/LoopConv.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConv.hpp"
#include "LoopConvCmdQueue.hpp"
#include "SmeshCommand.hpp"

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

  UPDATE(updateClassify)
      .reads(head_val_, head_bits_)
      .writes(is_loop_run_cmd_, is_loop_config_cmd_, is_loop_cmd_);
  UPDATE(updateRouting)
      .reads(head_val_, head_bits_, is_loop_cmd_, out_rdy)
      .writes(head_rdy_, out_val, out_bits, busy);
}

LoopConv::~LoopConv() {
  delete cmd_queue_;
}

// Recognize Original's launch function 15 and configuration functions 16..21.
void LoopConv::updateClassify() {
  const auto funct    = static_cast<std::uint32_t>((*head_bits_).cmd.funct);
  const bool run      = head_val_ == 1 && funct == static_cast<std::uint32_t>(SmeshFunct::LoopConvWs);
  const bool config   = head_val_ == 1 && funct >= static_cast<std::uint32_t>(SmeshFunct::LoopConvWsConfig1) && funct <= static_cast<std::uint32_t>(SmeshFunct::LoopConvWsConfig6);
  is_loop_run_cmd_    = bit(run);
  is_loop_config_cmd_ = bit(config);
  is_loop_cmd_        = bit(run || config);
}

// Send ordinary commands downstream; remove them only when the output accepts.
void LoopConv::updateRouting() {
  const bool pass_through = head_val_ == 1 && is_loop_cmd_ == 0;
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
  is_loop_run_cmd_.reset(0);
  is_loop_config_cmd_.reset(0);
  is_loop_cmd_.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshQueuedCmd{});
  busy.reset(0);
}

} // namespace smesh
