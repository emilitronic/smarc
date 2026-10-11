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

  // Decode the queue head before saving an accepted setup command.
  cmd_dec_ = new LoopConvCmdDec("CmdDec");
  cmd_dec_->clk << clk;
  cmd_dec_->head_val << head_val_;
  cmd_dec_->head_bits << head_bits_;
  decoded_ << cmd_dec_->decoded;

  loop_state_ = new LoopConvState("LoopState");
  loop_state_->clk << clk;
  loop_state_->decoded << decoded_;
  state_cmd_rdy_ << loop_state_->cmd_rdy;
  loop_configured_ << loop_state_->loop_configured;
  loop0 << loop_state_->loop0;
  loop1 << loop_state_->loop1;
  head_loop_id << loop_state_->head_loop_id;
  tail_loop_id << loop_state_->tail_loop_id;
  config_loop_id << loop_state_->config_loop_id;

  scheduler_ = new LoopConvScheduler("Scheduler");
  scheduler_->clk << clk;
  scheduler_->loop0 << loop_state_->loop0;
  scheduler_->loop1 << loop_state_->loop1;
  scheduler_->head_loop_id << loop_state_->head_loop_id;
  loop_state_->ld_bias_req_fire << scheduler_->ld_bias_req_fire;
  loop_state_->ld_bias_loop_id << scheduler_->ld_bias_loop_id;
  // The bias worker is not implemented yet; hold its request until it exists.
  scheduler_->ld_bias_req_rdy.wireToConst(bit(0));

  UPDATE(updateRouting)
      .reads(head_val_, head_bits_, decoded_, out_rdy, state_cmd_rdy_, loop_configured_)
      .writes(head_rdy_, out_val, out_bits, busy);
}

LoopConv::~LoopConv() {
  delete scheduler_;
  delete loop_state_;
  delete cmd_dec_;
  delete cmd_queue_;
}

// Consume accepted loop setup; pass ordinary commands only while no loop is configured.
void LoopConv::updateRouting() {
  const auto decoded = *decoded_;
  const bool loop_command = decoded.do_config == 1 || decoded.do_launch == 1;
  const bool pass_through = head_val_ == 1 && !loop_command && loop_configured_ == 0;
  out_val   = bit(pass_through);
  out_bits  = pass_through ? *head_bits_ : SmeshQueuedCmd{};
  head_rdy_ = bit(head_val_ == 1 &&
                  (loop_command ? state_cmd_rdy_ == 1 : pass_through && out_rdy == 1));
  busy = bit(head_val_ == 1 || loop_configured_ == 1);
  // TODO: Select worker-arbiter output while a loop is configured.
}

void LoopConv::reset() {
  head_rdy_.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshQueuedCmd{});
  busy.reset(0);
}

} // namespace smesh
