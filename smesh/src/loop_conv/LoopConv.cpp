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
  cmd_queue_->cmd_val  << in_val;
  cmd_queue_->cmd_bits << in_bits;
  in_rdy               << cmd_queue_->cmd_rdy;

  // The internal router now controls when the oldest command leaves the queue.
  head_val_            << cmd_queue_->head_val;
  head_bits_           << cmd_queue_->head_bits;
  cmd_queue_->head_rdy << head_rdy_;

  // Decode the queue head before saving an accepted setup command.
  cmd_dec_ = new LoopConvCmdDec("CmdDec");
  cmd_dec_->clk       << clk;
  cmd_dec_->head_val  << head_val_;
  cmd_dec_->head_bits << head_bits_;
  decoded_            << cmd_dec_->decoded;

  loop_state_ = new LoopConvState("LoopState");
  loop_state_->clk     << clk;
  loop_state_->decoded << decoded_;
  state_cmd_rdy_       << loop_state_->cmd_rdy;
  loop_configured_     << loop_state_->loop_configured;
  loop0                << loop_state_->loop0;
  loop1                << loop_state_->loop1;
  head_loop_id         << loop_state_->head_loop_id;
  tail_loop_id         << loop_state_->tail_loop_id;
  config_loop_id       << loop_state_->config_loop_id;

  scheduler_ = new LoopConvScheduler("Scheduler");
  scheduler_->clk               << clk;
  scheduler_->loop0             << loop_state_->loop0;
  scheduler_->loop1             << loop_state_->loop1;
  scheduler_->head_loop_id      << loop_state_->head_loop_id;
  // Keep weight jobs pending until the load-weights worker is implemented.
  scheduler_->ld_weights_req_rdy.wireToConst(bit(0));
  loop_state_->ld_bias_req_fire << scheduler_->ld_bias_req_fire;
  loop_state_->ld_bias_loop_id  << scheduler_->ld_bias_loop_id;
  loop_state_->ld_input_req_fire << scheduler_->ld_input_req_fire;
  loop_state_->ld_input_loop_id << scheduler_->ld_input_loop_id;
  ld_bias_ = new LoopConvLdBias("LdBias");
  ld_bias_->clk                       << clk;
  ld_bias_->req_val                   << scheduler_->ld_bias_req_val;
  ld_bias_->req_bits                  << scheduler_->ld_bias_req_bits;
  scheduler_->ld_bias_req_rdy         << ld_bias_->req_rdy;
  loop_state_->ld_bias_idle           << ld_bias_->idle;
  loop_state_->ld_bias_worker_loop_id << ld_bias_->loop_id;
  // TODO: Connect load utilization and execute-dependent waiting as they are added.
  ld_bias_->ld_utilization_at_limit.wireToConst(bit(0));
  ld_bias_->wait_for_prev_loop.wireToConst(bit(0));
  ld_bias_->cmd_rdy << ld_bias_cmd_rdy_;
  ld_bias_cmd_val_  << ld_bias_->cmd_val;
  ld_bias_cmd_bits_ << ld_bias_->cmd_bits;

  // Input loads run independently of bias loads and share the command output.
  ld_input_ = new LoopConvLdInput("LdInput");
  ld_input_->clk                       << clk;
  ld_input_->req_val                   << scheduler_->ld_input_req_val;
  ld_input_->req_bits                  << scheduler_->ld_input_req_bits;
  scheduler_->ld_input_req_rdy         << ld_input_->req_rdy;
  loop_state_->ld_input_idle           << ld_input_->idle;
  loop_state_->ld_input_worker_loop_id << ld_input_->loop_id;
  ld_input_->cmd_rdy                   << ld_input_cmd_rdy_;
  ld_input_cmd_val_                    << ld_input_->cmd_val;
  ld_input_cmd_bits_                   << ld_input_->cmd_bits;
  // TODO: Connect utilization and execute-dependent waiting when implemented.
  ld_input_->ld_utilization_at_limit.wireToConst(bit(0));
  ld_input_->wait_for_prev_loop.wireToConst(bit(0));

  UPDATE(updateRouting)
      .reads(head_val_, head_bits_, decoded_, out_rdy, state_cmd_rdy_, loop_configured_)
      .reads(ld_bias_cmd_val_, ld_bias_cmd_bits_)
      .reads(ld_input_cmd_val_, ld_input_cmd_bits_)
      .writes(head_rdy_, out_val, out_bits, busy);
  UPDATE(updateWorkerReady)
      .reads(loop_configured_, ld_bias_cmd_val_, ld_input_cmd_val_, out_rdy)
      .writes(ld_bias_cmd_rdy_, ld_input_cmd_rdy_);
}

LoopConv::~LoopConv() {
  delete ld_input_;
  delete ld_bias_;
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
  if (loop_configured_ == 1) {
    SmeshQueuedCmd generated{};
    // Original gives bias commands priority over input commands.
    const bool bias_selected = ld_bias_cmd_val_ == 1;
    generated.cmd            = bias_selected ? *ld_bias_cmd_bits_ : *ld_input_cmd_bits_;
    generated.from_conv_loop = 1;
    const bool valid         = bias_selected || ld_input_cmd_val_ == 1;
    out_bits                 = valid ? generated : SmeshQueuedCmd{};
    out_val                  = bit(valid);
  }
  head_rdy_ = bit(head_val_ == 1 && (loop_command ? state_cmd_rdy_ == 1 : pass_through && out_rdy == 1));
  busy      = bit(head_val_ == 1 || loop_configured_ == 1);
}

// Only the selected worker removes a command when downstream accepts it.
void LoopConv::updateWorkerReady() {
  const bool accept = loop_configured_ == 1 && out_rdy == 1;
  ld_bias_cmd_rdy_ = bit(accept && ld_bias_cmd_val_ == 1);
  ld_input_cmd_rdy_ = bit(accept && ld_bias_cmd_val_ == 0 && ld_input_cmd_val_ == 1);
}

void LoopConv::reset() {
  head_rdy_.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshQueuedCmd{});
  busy.reset(0);
  ld_bias_cmd_rdy_.reset(0);
  ld_input_cmd_rdy_.reset(0);
}

} // namespace smesh
