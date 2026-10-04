// **********************************************************************
// smesh/src/accum_response/Normalizer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
// Stores accepted accumulator packets by stats slot and returns Reset packets.

#include "Normalizer.hpp"

namespace smesh {

Normalizer::Normalizer(std::string /*name*/, IMPL_CTOR) {
  state_ = new NormState("State");
  state_->clk       << clk;

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

  saved_Q_ <= saved_D_;

  // Control req_rdy 
  UPDATE(updateAdmission).reads(req_val, req_bits, state_req_rdy_)
                         .writes(allowed_req_val_, req_rdy);
  // Gives NormState saved cmd for each slot                   
  UPDATE(updateSlotCmds).reads(saved_Q_).writes(slot_cmds_);
  // 
  UPDATE(updateEvents).writes(events_);
  // Uses NormState's selected output slot to present slot's saved packed on resp_bits
  UPDATE(updateView).reads(saved_Q_, state_out_val_, state_out_id_)
                    .writes(resp_val, resp_bits);
  // Saves accepted packet into slot selected by accept_id.  Saved value becomes current at next clock edge.                  
  UPDATE(updateSlots).reads(saved_Q_, req_bits, state_accept_val_, state_accept_id_)
                     .writes(saved_D_);
}

Normalizer::~Normalizer() {
  delete state_;
}

void Normalizer::updateAdmission() {
  const bool supported = req_bits->cmd.cmd == static_cast<std::uint8_t>(NormCmd::Reset);
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
  events_ = NormStateEvents{};
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
  req_rdy.reset(0);
  resp_val.reset(0);
  resp_bits.reset(AccNormReq{});
}

} // namespace smesh
