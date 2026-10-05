// **********************************************************************
// smesh/src/accum_response/Normalizer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
// Stores packets by stats slot and processes Reset, Sum, Max, Mean, and Variance commands.

#include "Normalizer.hpp"
#include "NormRowChunk.hpp"
#include "NormSumLane.hpp"
#include "NormMaxLane.hpp"
#include "NormMeanDivide.hpp"
#include "NormSqrt.hpp"
#include "NormReciprocal.hpp"
#include "NormStats.hpp"

#include <cstdint>

namespace smesh {

Normalizer::Normalizer(std::string /*name*/, std::size_t reduce_lanes, IMPL_CTOR) {
  state_       = new NormState("State");
  chunker_     = new NormRowChunk("SumChunk", reduce_lanes, NormFsmState::GetSum);
  max_chunker_ = new NormRowChunk("MaxChunk", reduce_lanes, NormFsmState::GetMax);
  sum_lane_    = new NormSumLane("SumLane");
  max_lane_    = new NormMaxLane("MaxLane");
  mean_divide_ = new NormMeanDivide("MeanDivide");
  sqrt_        = new NormSqrt("Sqrt");
  reciprocal_  = new NormReciprocal("Reciprocal");
  stats_       = new NormStats("Stats");
  state_->clk       << clk;
  chunker_->clk     << clk;
  max_chunker_->clk << clk;
  sum_lane_->clk    << clk;
  max_lane_->clk    << clk;
  mean_divide_->clk << clk;
  sqrt_->clk        << clk;
  reciprocal_->clk  << clk;
  stats_->clk       << clk;

  state_->req_val   << allowed_req_val_;
  state_->req_bits  << req_bits;
  state_->slot_cmds << slot_cmds_;
  state_->events    << events_;
  state_->out_rdy   << resp_rdy;
  state_req_rdy_    << state_->req_rdy;
  state_accept_val_ << state_->accept_val;
  state_accept_id_  << state_->accept_id;
  state_out_val_    << state_->out_val;
  state_out_id_     << state_->out_id;
  slot_states_      << state_->slot_states;

  chunker_->saved       << saved_Q_;
  chunker_->slot_states << state_->slot_states;
  chunker_->stats       << stats_->view;
  chunk_val_            << chunker_->chunk_val;
  chunk_bits_           << chunker_->chunk_bits;

  max_chunker_->saved       << saved_Q_;
  max_chunker_->slot_states << state_->slot_states;
  max_chunker_->stats       << stats_->view;
  max_chunk_val_            << max_chunker_->chunk_val;
  max_chunk_bits_           << max_chunker_->chunk_bits;

  sum_lane_->chunk_val  << chunker_->chunk_val;
  sum_lane_->chunk_bits << chunker_->chunk_bits;
  max_lane_->chunk_val  << max_chunker_->chunk_val;
  max_lane_->chunk_bits << max_chunker_->chunk_bits;

  mean_divide_->slot_states << state_->slot_states;
  mean_divide_->stats       << stats_->view;
  divide_started_           << mean_divide_->started;
  divide_start_id_          << mean_divide_->start_id;
  divide_finished_          << mean_divide_->finished;
  divide_finish_id_         << mean_divide_->finish_id;

  sqrt_->slot_states << state_->slot_states;
  sqrt_->stats       << stats_->view;
  sqrt_started_      << sqrt_->started;
  sqrt_start_id_     << sqrt_->start_id;
  sqrt_finished_     << sqrt_->finished;
  sqrt_finish_id_    << sqrt_->finish_id;

  reciprocal_->slot_states << state_->slot_states;
  reciprocal_->stats       << stats_->view;
  reciprocal_started_      << reciprocal_->started;
  reciprocal_start_id_     << reciprocal_->start_id;
  reciprocal_finished_     << reciprocal_->finished;
  reciprocal_finish_id_    << reciprocal_->finish_id;

  stats_->accept_val     << state_->accept_val;
  stats_->accept_id      << state_->accept_id;
  stats_->req_bits       << req_bits;
  stats_->slot_states    << state_->slot_states;
  stats_->sum_chunk_val  << chunker_->chunk_val;
  stats_->sum_chunk_bits << chunker_->chunk_bits;
  stats_->max_chunk_val  << max_chunker_->chunk_val;
  stats_->max_chunk_bits << max_chunker_->chunk_bits;
  stats_->sum_val        << sum_lane_->result_val;
  stats_->sum_bits       << sum_lane_->result_bits;
  stats_->max_val        << max_lane_->result_val;
  stats_->max_bits       << max_lane_->result_bits;
  stats_->divide_started   << mean_divide_->started;
  stats_->divide_start_id  << mean_divide_->start_id;
  stats_->divide_finished  << mean_divide_->finished;
  stats_->divide_finish_id << mean_divide_->finish_id;
  stats_->divide_result    << mean_divide_->result;
  stats_->sqrt_finished    << sqrt_->finished;
  stats_->sqrt_finish_id   << sqrt_->finish_id;
  stats_->sqrt_result      << sqrt_->result;
  stats_->reciprocal_finished  << reciprocal_->finished;
  stats_->reciprocal_finish_id << reciprocal_->finish_id;
  stats_->reciprocal_result    << reciprocal_->result;
  stats_view             << stats_->view;

  saved_Q_ <= saved_D_;

  // Control req_rdy 
  UPDATE(updateAdmission).reads(req_val, req_bits, state_req_rdy_)
                         .writes(allowed_req_val_, req_rdy);
  // Gives NormState saved cmd for each slot                   
  UPDATE(updateSlotCmds).reads(saved_Q_).writes(slot_cmds_);
  UPDATE(updateEvents).reads(slot_states_, stats_view, chunk_val_, chunk_bits_,
                             max_chunk_val_, max_chunk_bits_, divide_started_,
                             divide_start_id_)
                      .reads(divide_finished_, divide_finish_id_, sqrt_started_,
                             sqrt_start_id_, sqrt_finished_, sqrt_finish_id_,
                             reciprocal_started_, reciprocal_start_id_)
                      .reads(reciprocal_finished_, reciprocal_finish_id_)
                      .writes(events_);
  // Uses NormState's selected output slot to present slot's saved packed on resp_bits
  UPDATE(updateView).reads(saved_Q_, state_out_val_, state_out_id_, stats_view)
                    .writes(resp_val, resp_bits);
  // Saves accepted packet into slot selected by accept_id.  Saved value becomes current at next clock edge.                  
  UPDATE(updateSlots).reads(saved_Q_, req_bits, state_accept_val_, state_accept_id_)
                     .writes(saved_D_);
}

Normalizer::~Normalizer() {
  delete stats_;
  delete mean_divide_;
  delete sqrt_;
  delete reciprocal_;
  delete max_lane_;
  delete sum_lane_;
  delete max_chunker_;
  delete chunker_;
  delete state_;
}

void Normalizer::updateAdmission() {
  const auto cmd       = static_cast<NormCmd>(static_cast<std::uint8_t>(req_bits->cmd.cmd));
  const bool supported = cmd == NormCmd::Reset ||
      ((cmd == NormCmd::Sum || cmd == NormCmd::Max || cmd == NormCmd::Mean ||
        cmd == NormCmd::Variance) && req_bits->cmd.len <= kDim);
  allowed_req_val_     = bit(req_val == 1 && supported);
  req_rdy              = bit(supported && state_req_rdy_ == 1);
}

void Normalizer::updateSlotCmds() {
  NormSlotCmds cmds{};
  const auto saved = *saved_Q_;
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    cmds.cmd[id] = saved.packet[id].cmd.cmd;
  }
  slot_cmds_ = cmds;
}

void Normalizer::updateEvents() {
  NormStateEvents next{};
  const auto states   = *slot_states_;
  const auto progress = *stats_view;
  const auto chunk    = *chunk_bits_;
  const auto max_chunk = *max_chunk_bits_;
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state = static_cast<NormFsmState>(static_cast<std::uint8_t>(states.state[id]));
    if (state == NormFsmState::GetSum) {
      const bool empty      = progress.elems_left[id] == 0;
      const bool last_chunk = chunk_val_ == 1 && chunk.last == 1 && chunk.slot == id;
      next.slot[id].sum_last_issued = bit(empty || last_chunk);
    } else if (state == NormFsmState::GetMax) {
      const bool empty = progress.elems_left[id] == 0;
      const bool last_chunk = max_chunk_val_ == 1 && max_chunk.last == 1 && max_chunk.slot == id;
      next.slot[id].max_last_issued = bit(empty || last_chunk);
    } else if (state == NormFsmState::GetMean || state == NormFsmState::GetVariance) {
      next.slot[id].divide_started = bit(divide_started_ == 1 && divide_start_id_ == id);
    } else if (state == NormFsmState::WaitingForMean || state == NormFsmState::WaitingForVariance) {
      next.slot[id].divide_finished = bit(divide_finished_ == 1 && divide_finish_id_ == id);
    } else if (state == NormFsmState::GetStddev) {
      next.slot[id].sqrt_started = bit(sqrt_started_ == 1 && sqrt_start_id_ == id);
    } else if (state == NormFsmState::WaitingForStddev) {
      next.slot[id].sqrt_finished = bit(sqrt_finished_ == 1 && sqrt_finish_id_ == id);
    } else if (state == NormFsmState::GetInvStddev) {
      next.slot[id].reciprocal_started = bit(reciprocal_started_ == 1 && reciprocal_start_id_ == id);
    } else if (state == NormFsmState::WaitingForInvStddev) {
      next.slot[id].reciprocal_finished = bit(reciprocal_finished_ == 1 && reciprocal_finish_id_ == id);
    }
  }
  events_ = next;
}

void Normalizer::updateView() {
  const auto id    = static_cast<std::size_t>(static_cast<std::uint8_t>(*state_out_id_));
  const bool valid = state_out_val_ == 1 && id < kNormStatsSlots;
  resp_val         = bit(valid);
  AccNormReq response{};
  if (valid) {
    response = saved_Q_->packet[id];
    response.mean = stats_view->mean[id];
  }
  resp_bits = response;
}

void Normalizer::updateSlots() {
  if (state_accept_val_ == 0) return;
  const auto id = static_cast<std::size_t>(static_cast<std::uint8_t>(*state_accept_id_));
  if (id >= kNormStatsSlots) return;
  auto saved       = *saved_Q_;
  saved.packet[id] = *req_bits;
  saved_D_         = saved;
  trace("normalizer: accepted slot=%u cmd_id=%u\n",
        static_cast<unsigned>(id),
        static_cast<unsigned>(req_bits->acc_read_resp.cmd_id));
}

void Normalizer::reset() {
  saved_Q_.reset(NormSavedPackets{});
  saved_D_.reset(NormSavedPackets{});
  allowed_req_val_.reset(0);
  state_req_rdy_.reset(0);
  state_accept_val_.reset(0);
  state_accept_id_.reset(0);
  state_out_val_.reset(0);
  state_out_id_.reset(0);
  slot_cmds_.reset(NormSlotCmds{});
  events_.reset(NormStateEvents{});
  slot_states_.reset(NormStateRegs{});
  chunk_val_.reset(0);
  chunk_bits_.reset(NormChunk{});
  max_chunk_val_.reset(0);
  max_chunk_bits_.reset(NormChunk{});
  divide_started_.reset(0);
  divide_start_id_.reset(0);
  divide_finished_.reset(0);
  divide_finish_id_.reset(0);
  sqrt_started_.reset(0);
  sqrt_start_id_.reset(0);
  sqrt_finished_.reset(0);
  sqrt_finish_id_.reset(0);
  reciprocal_started_.reset(0);
  reciprocal_start_id_.reset(0);
  reciprocal_finished_.reset(0);
  reciprocal_finish_id_.reset(0);
  req_rdy.reset(0);
  resp_val.reset(0);
  resp_bits.reset(AccNormReq{});
  stats_view.reset(NormStatsRegs{});
}

} // namespace smesh
