// **********************************************************************
// smesh/src/loop_conv/LoopConv.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "LoopConv.hpp"
#include "LoopConvCmdQueue.hpp"
#include "LoopConvUtilization.hpp"

namespace smesh {

LoopConv::LoopConv(std::string /*name*/, IMPL_CTOR) {
  utilization_ = new LoopConvUtilization("Utilization");
  utilization_->clk          << clk;
  utilization_->ld_completed << ld_completed;
  utilization_->ex_completed << ex_completed;
  utilization_->st_completed << st_completed;
  utilization_->ld_cmd_fire  << ld_cmd_fire_;
  utilization_->ex_cmd_fire  << ex_cmd_fire_;
  utilization_->st_cmd_fire  << st_cmd_fire_;
  ld_outstanding             << utilization_->ld_outstanding;
  ex_outstanding             << utilization_->ex_outstanding;
  st_outstanding             << utilization_->st_outstanding;
  cmd_queue_ = new LoopConvCmdQueue("CmdQueue");
  cmd_queue_->clk            << clk;

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
  loop_state_->st_req_fire         << scheduler_->st_req_fire;
  loop_state_->st_loop_id          << scheduler_->st_loop_id;
  loop_state_->ex_req_fire         << scheduler_->ex_req_fire;
  loop_state_->ex_loop_id          << scheduler_->ex_loop_id;
  loop_state_->ld_weights_req_fire << scheduler_->ld_weights_req_fire;
  loop_state_->ld_weights_loop_id  << scheduler_->ld_weights_loop_id;
  loop_state_->ld_bias_req_fire    << scheduler_->ld_bias_req_fire;
  loop_state_->ld_bias_loop_id     << scheduler_->ld_bias_loop_id;
  loop_state_->ld_input_req_fire   << scheduler_->ld_input_req_fire;
  loop_state_->ld_input_loop_id    << scheduler_->ld_input_loop_id;
  ld_bias_ = new LoopConvLdBias("LdBias");
  ld_bias_->clk                       << clk;
  ld_bias_->req_val                   << scheduler_->ld_bias_req_val;
  ld_bias_->req_bits                  << scheduler_->ld_bias_req_bits;
  scheduler_->ld_bias_req_rdy         << ld_bias_->req_rdy;
  loop_state_->ld_bias_idle           << ld_bias_->idle;
  loop_state_->ld_bias_worker_loop_id << ld_bias_->loop_id;
  ld_bias_->ld_utilization_at_limit   << utilization_->ld_utilization_at_limit;
  ld_bias_->wait_for_prev_loop        << bias_wait_;
  ld_bias_->cmd_rdy                   << ld_bias_cmd_rdy_;
  ld_bias_cmd_val_                    << ld_bias_->cmd_val;
  ld_bias_cmd_bits_                   << ld_bias_->cmd_bits;

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
  ld_input_->ld_utilization_at_limit << utilization_->ld_utilization_at_limit;
  ld_input_->wait_for_prev_loop << input_wait_;

  // Weight loads use the scheduler's SPAD end address and load-state slot 1.
  ld_weights_ = new LoopConvLdWeight("LdWeight");
  ld_weights_->clk                       << clk;
  ld_weights_->req_val                   << scheduler_->ld_weights_req_val;
  ld_weights_->req_bits                  << scheduler_->ld_weights_req_bits;
  scheduler_->ld_weights_req_rdy         << ld_weights_->req_rdy;
  loop_state_->ld_weights_idle           << ld_weights_->idle;
  loop_state_->ld_weights_worker_loop_id << ld_weights_->loop_id;
  ld_weights_->cmd_rdy                   << ld_weights_cmd_rdy_;
  ld_weights_cmd_val_                    << ld_weights_->cmd_val;
  ld_weights_cmd_bits_                   << ld_weights_->cmd_bits;
  ld_weights_->ld_utilization_at_limit   << utilization_->ld_utilization_at_limit;
  ld_weights_->wait_for_prev_loop        << weights_wait_;

  // Execute accepts a job after its loads start, then waits for their drain.
  ex_ = new LoopConvExecute("Execute");
  ex_->clk                       << clk;
  ex_->req_val                   << scheduler_->ex_req_val;
  ex_->req_bits                  << scheduler_->ex_req_bits;
  scheduler_->ex_req_rdy         << ex_->req_rdy;
  loop_state_->ex_idle           << ex_->idle;
  loop_state_->ex_worker_loop_id << ex_->loop_id;
  ex_->lda_completed             << lda_completed_;
  ex_->ldb_completed             << ldb_completed_;
  ex_->ldd_completed             << ldd_completed_;
  ex_->cmd_rdy                   << ex_cmd_rdy_;
  ex_cmd_val_                    << ex_->cmd_val;
  ex_cmd_bits_                   << ex_->cmd_bits;
  ex_->ex_utilization_at_limit   << utilization_->ex_utilization_at_limit;
  ex_worker_loop_id_             << ex_->loop_id;
  bias_worker_loop_id_           << ld_bias_->loop_id;
  input_worker_loop_id_          << ld_input_->loop_id;
  weights_worker_loop_id_        << ld_weights_->loop_id;
  bias_idle_                     << ld_bias_->idle;
  input_idle_                    << ld_input_->idle;
  weights_idle_                  << ld_weights_->idle;
  progress_loop0_                << loop_state_->loop0;
  progress_loop1_                << loop_state_->loop1;

  // Store accepts its job after Execute starts, then waits for command drain.
  st_ = new LoopConvSt("St");
  st_->clk                       << clk;
  st_->req_val                   << scheduler_->st_req_val;
  st_->req_bits                  << scheduler_->st_req_bits;
  scheduler_->st_req_rdy         << st_->req_rdy;
  loop_state_->st_idle           << st_->idle;
  loop_state_->st_worker_loop_id << st_->loop_id;
  st_->ex_completed              << store_ex_completed_;
  st_->cmd_rdy                   << st_cmd_rdy_;
  st_cmd_val_                    << st_->cmd_val;
  st_cmd_bits_                   << st_->cmd_bits;
  st_worker_loop_id_             << st_->loop_id;
  ex_idle_                       << ex_->idle;
  st_->st_utilization_at_limit   << utilization_->st_utilization_at_limit;

  UPDATE(updateRouting)
      .reads(head_val_, head_bits_, decoded_, out_rdy, state_cmd_rdy_, loop_configured_)
      .reads(ld_bias_cmd_val_, ld_bias_cmd_bits_)
      .reads(ld_input_cmd_val_, ld_input_cmd_bits_)
      .reads(ld_weights_cmd_val_, ld_weights_cmd_bits_)
      .reads(ex_cmd_val_, ex_cmd_bits_)
      .reads(st_cmd_val_, st_cmd_bits_)
      .writes(head_rdy_, out_val, out_bits, busy);
  UPDATE(updateWorkerReady)
      .reads(loop_configured_, ld_bias_cmd_val_, ld_input_cmd_val_, out_rdy)
      .reads(ld_weights_cmd_val_, ex_cmd_val_, st_cmd_val_)
      .writes(ld_bias_cmd_rdy_, ld_input_cmd_rdy_, ld_weights_cmd_rdy_, ex_cmd_rdy_, st_cmd_rdy_);
  UPDATE(updateExecuteProgress)
      .reads(ex_worker_loop_id_, bias_worker_loop_id_, input_worker_loop_id_, weights_worker_loop_id_)
      .reads(bias_idle_, input_idle_, weights_idle_, progress_loop0_, progress_loop1_)
      .writes(lda_completed_, ldb_completed_, ldd_completed_, bias_wait_, input_wait_, weights_wait_);
  UPDATE(updateStoreProgress).reads(ex_worker_loop_id_, st_worker_loop_id_, ex_idle_)
      .writes(store_ex_completed_);
  UPDATE(updateCommandTransfers)
      .reads(ld_bias_cmd_rdy_, ld_input_cmd_rdy_, ld_weights_cmd_rdy_, ex_cmd_rdy_, st_cmd_rdy_)
      .writes(ld_cmd_fire_, ex_cmd_fire_, st_cmd_fire_);
}

LoopConv::~LoopConv() {
  delete utilization_;
  delete st_;
  delete ex_;
  delete ld_weights_;
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
    // Original's priority is Store, Execute, bias, weights, then input.
    const bool bias_selected = ld_bias_cmd_val_ == 1;
    generated.cmd            = st_cmd_val_ == 1 ? *st_cmd_bits_ : ex_cmd_val_ == 1 ? *ex_cmd_bits_ : bias_selected ? *ld_bias_cmd_bits_ :
                               ld_weights_cmd_val_ == 1 ? *ld_weights_cmd_bits_ : *ld_input_cmd_bits_;
    generated.from_conv_loop = 1;
    const bool valid         = st_cmd_val_ == 1 || ex_cmd_val_ == 1 || bias_selected || ld_weights_cmd_val_ == 1 || ld_input_cmd_val_ == 1;
    out_bits                 = valid ? generated : SmeshQueuedCmd{};
    out_val                  = bit(valid);
  }
  head_rdy_ = bit(head_val_ == 1 && (loop_command ? state_cmd_rdy_ == 1 : pass_through && out_rdy == 1));
  busy      = bit(head_val_ == 1 || loop_configured_ == 1);
}

// Only the selected worker removes a command when downstream accepts it.
void LoopConv::updateWorkerReady() {
  st_cmd_rdy_         = bit(loop_configured_ == 1 && out_rdy == 1 && st_cmd_val_ == 1);
  ex_cmd_rdy_         = bit(loop_configured_ == 1 && out_rdy == 1 && st_cmd_val_ == 0 && ex_cmd_val_ == 1);
  const bool accept   = loop_configured_ == 1 && out_rdy == 1 && st_cmd_val_ == 0 && ex_cmd_val_ == 0;
  ld_bias_cmd_rdy_    = bit(accept && ld_bias_cmd_val_ == 1);
  ld_weights_cmd_rdy_ = bit(accept && ld_bias_cmd_val_ == 0 && ld_weights_cmd_val_ == 1);
  ld_input_cmd_rdy_   = bit(accept && ld_bias_cmd_val_ == 0 && ld_weights_cmd_val_ == 0 && ld_input_cmd_val_ == 1);
}

// A loader that moved to another loop has already drained this Execute job's
// commands. Pause that other loop's generation while Execute awaits its loads.
void LoopConv::updateExecuteProgress() {
  const auto id      = static_cast<unsigned>(*ex_worker_loop_id_);
  const auto slot    = id == 0 ? *progress_loop0_ : *progress_loop1_;
  lda_completed_     = bit(input_worker_loop_id_ != ex_worker_loop_id_ || input_idle_ == 1);
  ldb_completed_     = bit(weights_worker_loop_id_ != ex_worker_loop_id_ || weights_idle_ == 1);
  ldd_completed_     = bit(bias_worker_loop_id_ != ex_worker_loop_id_ || bias_idle_ == 1);
  const bool waiting = slot.ex_started == 1 && slot.ex_completed == 0 && !(slot.ld_bias_completed == 1 && slot.ld_input_completed == 1 && slot.ld_weights_completed == 1);
  bias_wait_         = bit(waiting && bias_worker_loop_id_ != ex_worker_loop_id_);
  input_wait_        = bit(waiting && input_worker_loop_id_ != ex_worker_loop_id_);
  weights_wait_      = bit(waiting && weights_worker_loop_id_ != ex_worker_loop_id_);
}

void LoopConv::reset() {
  ld_cmd_fire_.reset(0);
  ex_cmd_fire_.reset(0);
  st_cmd_fire_.reset(0);
  head_rdy_.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshQueuedCmd{});
  busy.reset(0);
  ld_bias_cmd_rdy_.reset(0);
  ld_input_cmd_rdy_.reset(0);
  ld_weights_cmd_rdy_.reset(0);
  ex_cmd_rdy_.reset(0);
  lda_completed_.reset(0);
  ldb_completed_.reset(0);
  ldd_completed_.reset(0);
  bias_wait_.reset(0);
  input_wait_.reset(0);
  weights_wait_.reset(0);
  st_cmd_rdy_.reset(0);
  store_ex_completed_.reset(0);
}

// A different Execute loop ID means this Store job's Execute pipeline drained
// before it accepted another job. Otherwise wait for that pipeline to be idle.
void LoopConv::updateStoreProgress() {
  const auto id       = static_cast<unsigned>(*st_worker_loop_id_);
  const auto slot     = id == 0 ? *progress_loop0_ : *progress_loop1_;
  store_ex_completed_ = bit(ex_worker_loop_id_ != st_worker_loop_id_ || ex_idle_ == 1);
}

// Worker ready is asserted only for the selected valid command when out_rdy
// is high. Thus these are actual worker transfers, excluding pass-through.
void LoopConv::updateCommandTransfers() {
  ld_cmd_fire_ = bit(ld_bias_cmd_rdy_ == 1 || ld_input_cmd_rdy_ == 1 || ld_weights_cmd_rdy_ == 1);
  ex_cmd_fire_ = ex_cmd_rdy_;
  st_cmd_fire_ = st_cmd_rdy_;
}

} // namespace smesh
