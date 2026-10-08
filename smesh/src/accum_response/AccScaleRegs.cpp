// **********************************************************************
// smesh/src/accum_response/AccScaleRegs.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026

#include "AccScaleRegs.hpp"

TraceKey(acc_scale_regs_);

namespace smesh {

constexpr std::size_t AccScaleRegs::kEntries;
constexpr std::size_t AccScaleRegs::kWidth;

AccScaleRegs::AccScaleRegs(std::string /*name*/, IMPL_CTOR) {
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    regs_val_Q_[slot]  <= regs_val_D_[slot];
    regs_bits_Q_[slot] <= regs_bits_D_[slot];
  }
  tail_oh_Q_ <= tail_oh_D_;
  UPDATE(updateReady).reads(regs_val_Q_, tail_oh_Q_).writes(req_rdy);
  UPDATE(updateRegs).reads(req_val, req_rdy, req_bits, tail_oh_Q_)
                    .writes(regs_val_D_, regs_bits_D_, tail_oh_D_);
}

// Accept an input only while the slot selected by tail_oh is empty.
void AccScaleRegs::updateReady() {
  const auto tail = static_cast<std::uint8_t>(*tail_oh_Q_);
  req_rdy = 0;
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    if ((tail & (1u << slot)) != 0) {
      req_rdy = bit(regs_val_Q_[slot] == 0);
    }
  }
}

// Capture the entire input packet in the selected slot on req_val && req_rdy.
void AccScaleRegs::updateRegs() {
  if (!(req_val == 1 && req_rdy == 1)) return;

  const auto tail = static_cast<std::uint8_t>(*tail_oh_Q_);
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    if ((tail & (1u << slot)) != 0) {
      regs_bits_D_[slot] = *req_bits;
      regs_val_D_[slot]  = 1;
      trace(acc_scale_regs_, "capture slot=%u elements=%u cmd_id=%u\n",
            static_cast<unsigned>(slot), static_cast<unsigned>(kWidth),
            static_cast<unsigned>(req_bits->norm.acc_read_resp.cmd_id));
    }
  }
  // Rotate the one-hot selection 001 -> 010 -> 100 -> 001; u3 keeps three bits.
  tail_oh_D_ = u3((tail << 1) | (tail >> (kEntries - 1)));
}

void AccScaleRegs::reset() {
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    regs_val_Q_[slot].reset(0);
    regs_val_D_[slot].reset(0);
    regs_bits_Q_[slot].reset(AccScaleReq{});
    regs_bits_D_[slot].reset(AccScaleReq{});
  }
  tail_oh_Q_.reset(1);
  tail_oh_D_.reset(1);
  req_rdy.reset(0);
}

} // namespace smesh
