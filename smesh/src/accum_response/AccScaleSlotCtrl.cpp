// **********************************************************************
// smesh/src/accum_response/AccScaleSlotCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026

#include "AccScaleSlotCtrl.hpp"

TraceKey(acc_scale_slot_ctrl_);

namespace smesh {

constexpr std::size_t AccScaleSlotCtrl::kEntries;
constexpr std::size_t AccScaleSlotCtrl::kWidth;

AccScaleSlotCtrl::AccScaleSlotCtrl(std::string /*name*/, IMPL_CTOR) {
  head_oh_Q_ <= head_oh_D_;
  tail_oh_Q_ <= tail_oh_D_;
  UPDATE(updateOutValid).reads(head_oh_Q_, regs_val, completed_masks).writes(out_val);
  UPDATE(updateOutTransfer).reads(out_val, out_rdy, head_oh_Q_)
                           .writes(out_fire, head_oh_D_);
  UPDATE(updateReqReady).reads(tail_oh_Q_, head_oh_Q_, regs_val, out_fire).writes(req_rdy);
  UPDATE(updateReqTransfer).reads(req_val, req_rdy, tail_oh_Q_)
                           .writes(req_fire, tail_oh_D_);
}

// Offer the oldest occupied slot only after every element has completed.
void AccScaleSlotCtrl::updateOutValid() {
  const auto head = static_cast<std::uint8_t>(*head_oh_Q_);
  out_val = 0;
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    if ((head & (1u << slot)) != 0) {
      const auto completed = *completed_masks[slot];
      bool complete = regs_val[slot] == 1;
      for (std::size_t element = 0; element < kWidth; ++element) {
        complete = complete && completed[element] == 1;
      }
      out_val = bit(complete);
    }
  }
}

// A successful output transfer releases this head slot and advances the pointer.
void AccScaleSlotCtrl::updateOutTransfer() {
  const bool fire = out_val == 1 && out_rdy == 1;
  out_fire = bit(fire);
  if (fire) {
    const auto head = static_cast<std::uint8_t>(*head_oh_Q_);
    // Keep exactly three bits; u3 can use native byte storage in Cascade.
    head_oh_D_ = u3(((head << 1) | (head >> (kEntries - 1))) & ((1u << kEntries) - 1u));
    trace(acc_scale_slot_ctrl_, "release head_oh=%u\n", static_cast<unsigned>(head));
  }
}

// The tail slot must be empty, or be the head slot leaving in this cycle.
void AccScaleSlotCtrl::updateReqReady() {
  const auto tail = static_cast<std::uint8_t>(*tail_oh_Q_);
  const auto head = static_cast<std::uint8_t>(*head_oh_Q_);
  req_rdy = 0;
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    if ((tail & (1u << slot)) != 0) {
      req_rdy = bit(regs_val[slot] == 0 || (tail == head && out_fire == 1));
    }
  }
}

// A successful input transfer stores in this tail slot and advances the pointer.
void AccScaleSlotCtrl::updateReqTransfer() {
  const bool fire = req_val == 1 && req_rdy == 1;
  req_fire = bit(fire);
  if (fire) {
    const auto tail = static_cast<std::uint8_t>(*tail_oh_Q_);
    tail_oh_D_ = u3(((tail << 1) | (tail >> (kEntries - 1))) & ((1u << kEntries) - 1u));
    trace(acc_scale_slot_ctrl_, "store tail_oh=%u\n", static_cast<unsigned>(tail));
  }
}

void AccScaleSlotCtrl::reset() {
  head_oh_Q_.reset(1);
  head_oh_D_.reset(1);
  tail_oh_Q_.reset(1);
  tail_oh_D_.reset(1);
  req_rdy.reset(0);
  req_fire.reset(0);
  out_val.reset(0);
  out_fire.reset(0);
}

} // namespace smesh
