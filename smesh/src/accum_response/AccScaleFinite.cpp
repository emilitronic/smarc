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

constexpr std::size_t AccScaleFinite::kLanes;

AccScaleFinite::AccScaleFinite(std::string /*name*/, int pipe_latency, bool has_nonlinear_activations, bool has_normalizations, IMPL_CTOR) {
  ctrl_ = new AccScaleSlotCtrl("SlotCtrl");
  regs_ = new AccScaleRegs("Regs");
  work_class_ = new AccScaleWorkClass("WorkClass", kLanes, has_normalizations ? kLanes : 0);

  ctrl_->clk << clk;
  regs_->clk << clk;
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

  for (std::size_t slot = 0; slot < AccScaleRegs::kEntries; ++slot) {
    ctrl_->regs_val[slot]        << regs_->regs_val_Q_[slot];
    ctrl_->completed_masks[slot] << regs_->completed_masks_Q_[slot];
    work_class_->regs_val[slot]  << regs_->regs_val_Q_[slot];
    work_class_->regs_bits[slot] << regs_->regs_bits_Q_[slot];
  }

  // Each arbiter sends one selected element through its own functional pipe.
  for (std::size_t lane = 0; lane < kLanes; ++lane) {
    lanes_[lane] = new AccScaleLane("Lane" + std::to_string(lane), lane, kLanes, has_normalizations); // interconnect component for current lane
    pipes_[lane] = new AccScalePipe("Pipe" + std::to_string(lane), pipe_latency,
                                   has_nonlinear_activations, has_normalizations); // functional component for current lane
    auto& arbiter = *lanes_[lane]; // pointer to current lane's arbiter and its arbOut register
    auto& pipe    = *pipes_[lane]; // pointer to current lane's functional pipe

    arbiter.clk << clk;
    pipe.clk    << clk;

    arbiter.req_fire << ctrl_->req_fire;   // tell lane when new row is accepted into input scaler's input regs
    arbiter.tail_oh  << ctrl_->tail_oh_Q_; // tell lane which input slot is accepting the new row (so completed marks can be cleared)

    // Work classification tells this arbiter which saved rows use its lane group.
    arbiter.current_policy << work_class_->current_policy;
    for (std::size_t slot = 0; slot < AccScaleRegs::kEntries; ++slot) {
      arbiter.regs_val[slot]  << regs_->regs_val_Q_[slot];
      arbiter.regs_bits[slot] << regs_->regs_bits_Q_[slot];
    }
    pipe.in_val  << arbiter.arb_out_val_Q_;
    pipe.in_bits << arbiter.arb_out_bits_Q_;
    // The pipe carries the slot/element address used to write its result to out_regs.
    regs_->result_val[lane]  << pipe.out_val;
    regs_->result_bits[lane] << pipe.out_bits;
  }

  UPDATE(updateReady).reads(out_bits, out_rdy_issue, out_rdy_exresp).writes(selected_out_rdy_);
}

AccScaleFinite::~AccScaleFinite() {
  for (std::size_t lane = 0; lane < kLanes; ++lane) {
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

void AccScaleFinite::reset() {
  selected_out_rdy_.reset(0);
  // Each child resets the row slots, masks, pointers, and pipe stages it owns.
}

} // namespace smesh
