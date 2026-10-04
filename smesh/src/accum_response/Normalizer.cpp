// **********************************************************************
// smesh/src/accum_response/Normalizer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
// Stores packets by stats slot and processes Reset, Sum, and Max commands.

#include "Normalizer.hpp"
#include "NormRowChunk.hpp"
#include "NormSumLane.hpp"
#include "NormMaxLane.hpp"
#include "NormStats.hpp"

#include <cstdint>

namespace smesh {

Normalizer::Normalizer(std::string /*name*/, std::size_t reduce_lanes, IMPL_CTOR) {
  state_ = new NormState("State");
  chunker_ = new NormRowChunk("SumChunk", reduce_lanes, NormFsmState::GetSum);
  max_chunker_ = new NormRowChunk("MaxChunk", reduce_lanes, NormFsmState::GetMax);
  sum_lane_ = new NormSumLane("SumLane");
  max_lane_ = new NormMaxLane("MaxLane");
  stats_ = new NormStats("Stats");
  state_->clk       << clk;
  chunker_->clk     << clk;
  max_chunker_->clk << clk;
  sum_lane_->clk    << clk;
  max_lane_->clk    << clk;
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
  stats_view             << stats_->view;

  saved_Q_ <= saved_D_;

  // Control req_rdy 
  UPDATE(updateAdmission).reads(req_val, req_bits, state_req_rdy_)
                         .writes(allowed_req_val_, req_rdy);
  // Gives NormState saved cmd for each slot                   
  UPDATE(updateSlotCmds).reads(saved_Q_).writes(slot_cmds_);
  UPDATE(updateEvents).reads(slot_states_, stats_view, chunk_val_, chunk_bits_,
                             max_chunk_val_, max_chunk_bits_)
                      .writes(events_);
  // Uses NormState's selected output slot to present slot's saved packed on resp_bits
  UPDATE(updateView).reads(saved_Q_, state_out_val_, state_out_id_)
                    .writes(resp_val, resp_bits);
  // Saves accepted packet into slot selected by accept_id.  Saved value becomes current at next clock edge.                  
  UPDATE(updateSlots).reads(saved_Q_, req_bits, state_accept_val_, state_accept_id_)
                     .writes(saved_D_);
}

Normalizer::~Normalizer() {
  delete stats_;
  delete max_lane_;
  delete sum_lane_;
  delete max_chunker_;
  delete chunker_;
  delete state_;
}

void Normalizer::updateAdmission() {
  const auto cmd       = static_cast<NormCmd>(static_cast<std::uint8_t>(req_bits->cmd.cmd));
  const bool supported = cmd == NormCmd::Reset || ((cmd == NormCmd::Sum || cmd == NormCmd::Max) && req_bits->cmd.len <= kDim);
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
    }
  }
  events_ = next;
}

void Normalizer::updateView() {
  const auto id    = static_cast<std::size_t>(static_cast<std::uint8_t>(*state_out_id_));
  const bool valid = state_out_val_ == 1 && id < kNormStatsSlots;
  resp_val         = bit(valid);
  resp_bits        = valid ? saved_Q_->packet[id] : AccNormReq{};
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
  req_rdy.reset(0);
  resp_val.reset(0);
  resp_bits.reset(AccNormReq{});
  stats_view.reset(NormStatsRegs{});
}

} // namespace smesh
