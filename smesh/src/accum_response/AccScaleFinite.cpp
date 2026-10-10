// **********************************************************************
// smesh/src/accum_response/AccScaleFinite.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026

#include "AccScaleFinite.hpp"
#include "AccScaleSlotCtrl.hpp"
#include "AccScaleLane.hpp"
#include "AccScalePipe.hpp"
#include "AccScaleWorkClass.hpp"

namespace smesh {

TraceKey(acc_scale_dispatch_);

constexpr std::size_t AccScaleFinite::kDefaultLanes;

AccScaleFinite::AccScaleFinite(std::string /*name*/, int pipe_latency, bool has_nonlinear_activations, bool has_normalizations, unsigned total_lanes, IMPL_CTOR)
    : lanes_(total_lanes), pipes_(total_lanes), dispatch_val_(total_lanes), dispatch_bits_(total_lanes) {
  assert_always(total_lanes > 0 && (!has_normalizations || total_lanes >= 4), "AccScaleFinite needs at least four lanes when normalization is enabled");
  const unsigned norm_lanes     = has_normalizations ? 4 : 0; // number of lanes able to normalize their input elements; the rest are ordinary lanes
  const unsigned ordinary_lanes = total_lanes - norm_lanes;   // number of ortinary lanes (but everybody's special)

  ctrl_       = new AccScaleSlotCtrl("SlotCtrl");
  regs_       = new AccScaleRegs("Regs", norm_lanes, ordinary_lanes);
  work_class_ = new AccScaleWorkClass("WorkClass", total_lanes, norm_lanes);

  ctrl_->clk       << clk;
  regs_->clk       << clk;
  work_class_->clk << clk;

  // Slot control accepts the input row and releases the completed output row.
  ctrl_->req_val  << req_val;
  req_rdy         << ctrl_->req_rdy;
  out_val         << ctrl_->out_val;
  out_bits        << regs_->out_bits;
  ctrl_->out_rdy  << selected_out_rdy_;

  regs_->req_bits << req_bits;
  regs_->req_fire << ctrl_->req_fire;
  regs_->tail_oh  << ctrl_->tail_oh_Q_;
  regs_->out_fire << ctrl_->out_fire;
  regs_->head_oh  << ctrl_->head_oh_Q_;
  req_fire_       << ctrl_->req_fire;
  tail_oh_        << ctrl_->tail_oh_Q_;

  for (std::size_t slot = 0; slot < AccScaleRegs::kEntries; ++slot) {
    ctrl_->regs_val[slot]        << regs_->regs_val_Q_[slot];
    ctrl_->completed_masks[slot] << regs_->completed_masks_Q_[slot];
    work_class_->regs_val[slot]  << regs_->regs_val_Q_[slot];
    work_class_->regs_bits[slot] << regs_->regs_bits_Q_[slot];
    fired_masks_Q_[slot]         <= fired_masks_D_[slot];
  }

  // Each arbiter sends one selected element through its own functional pipe.
  for (unsigned lane = 0; lane < total_lanes; ++lane) {
    const bool norm_lane   = lane < norm_lanes;
    const auto group_index = norm_lane ? lane : lane - norm_lanes;
    const auto group_size  = norm_lane ? norm_lanes : ordinary_lanes;
    lanes_[lane]           = new AccScaleLane("Lane" + std::to_string(lane), group_index, group_size, norm_lane, true); // interconnect component for current lane
    pipes_[lane]           = new AccScalePipe("Pipe" + std::to_string(lane), pipe_latency, has_nonlinear_activations, norm_lane); // functional component for current lane
    auto& arbiter          = *lanes_[lane]; // pointer to current lane's arbiter and its arbOut register
    auto& pipe             = *pipes_[lane]; // pointer to current lane's functional pipe

    arbiter.clk << clk;
    pipe.clk    << clk;

    arbiter.req_fire << ctrl_->req_fire;   // tell lane when new row is accepted into input scaler's input regs
    arbiter.tail_oh  << ctrl_->tail_oh_Q_; // tell lane which input slot is accepting the new row (so completed marks can be cleared)

    // Work classification tells this arbiter which saved rows use its lane group.
    arbiter.current_policy << work_class_->current_policy;
    for (std::size_t slot = 0; slot < AccScaleRegs::kEntries; ++slot) {
      arbiter.regs_val[slot]           << regs_->regs_val_Q_[slot];
      arbiter.regs_bits[slot]          << regs_->regs_bits_Q_[slot];
      arbiter.shared_fired_masks[slot] << fired_masks_Q_[slot];
    }
    pipe.in_val  << arbiter.arb_out_val_Q_;
    pipe.in_bits << arbiter.arb_out_bits_Q_;
    // The pipe carries the slot/element address used to write its result to out_regs.
    regs_->result_val[lane]  << pipe.out_val;
    regs_->result_bits[lane] << pipe.out_bits;
    dispatch_val_[lane]      << arbiter.arb_val;
    dispatch_bits_[lane]     << arbiter.arb_bits;
  }

  UPDATE(updateReady).reads(out_bits, out_rdy_issue, out_rdy_exresp).writes(selected_out_rdy_);
  UPDATE(updateDispatch).reads(req_fire_, tail_oh_, fired_masks_Q_, dispatch_val_, dispatch_bits_).writes(fired_masks_D_);
}

AccScaleFinite::~AccScaleFinite() {
  for (std::size_t lane = 0; lane < lanes_.size(); ++lane) {
    delete pipes_[lane];
    delete lanes_[lane];
  }
  delete regs_;
  delete ctrl_;
  delete work_class_;
}

// A store row waits for StIssueCtrl; an execute row waits for AccumExResp.
void AccScaleFinite::updateReady() {
  selected_out_rdy_ = out_bits->from_dma == 1 ? *out_rdy_issue : *out_rdy_exresp;
}

// Track which row elements have entered a lane, so reassignment cannot send them twice.
void AccScaleFinite::updateDispatch() {
  const auto tail = static_cast<std::uint8_t>(*tail_oh_); // which slot is accepting the new input row
  // Visit each slot
  for (std::size_t slot = 0; slot < AccScaleRegs::kEntries; ++slot) {
    auto fired   = *fired_masks_Q_[slot]; // which elements of this slot have been dispatched to the lanes
    bool changed = false;                 // assume no change to the dispatched record for this slot
    // is a new row being accepted into this slot? If so, clear its dispatched record
    if (req_fire_ == 1 && (tail & (1u << slot)) != 0) {
      fired   = AccScaleLane::FiredMask{};
      changed = true;                     // mark the dispatched record as changed for this slot
    }
    // Collect this cycle's dispatches from all the lane arbiters and mark them as dispatched if they are from this slot.
    for (std::size_t lane = 0; lane < lanes_.size(); ++lane) {
      if (dispatch_val_[lane] == 0) continue; // no accepted element from this lane this cycle? go to next lane
      const auto element = *dispatch_bits_[lane];
      if (element.slot != slot) continue;     // accepted element did not come from this slot? go to next lane
      const auto index = static_cast<std::uint16_t>(element.element); // get element's index within slot's row
      assert_always(index < AccScaleRegs::kWidth, "AccScale dispatch names an invalid element");
      assert_always(!(req_fire_ == 1 && (tail & (1u << slot)) != 0), "Cannot dispatch while replacing a row");
      assert_always(fired[index] == 0, "AccScale element dispatched more than once across lane groups");
      fired[index] = 1;     // record that this element has been dispatched to a lane (i.e., left the slot)
      changed      = true;  
      trace(acc_scale_dispatch_, "send lane=%u slot=%u element=%u act=%u\n",
            static_cast<unsigned>(lane), static_cast<unsigned>(slot),
            static_cast<unsigned>(index), static_cast<unsigned>(element.act));
    }
    if (changed) fired_masks_D_[slot] = fired; // update slot's dispatched record if it changed this cycle
  }
}

void AccScaleFinite::reset() {
  selected_out_rdy_.reset(0);
  for (std::size_t slot = 0; slot < AccScaleRegs::kEntries; ++slot) {
    fired_masks_Q_[slot].reset(AccScaleLane::FiredMask{});
    fired_masks_D_[slot].reset(AccScaleLane::FiredMask{});
  }
  // Each child resets the row slots, masks, pointers, and pipe stages it owns.
}

} // namespace smesh
