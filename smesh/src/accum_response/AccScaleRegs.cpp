// **********************************************************************
// smesh/src/accum_response/AccScaleRegs.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026

#include "AccScaleRegs.hpp"

TraceKey(acc_scale_regs_);

namespace smesh {

constexpr std::size_t AccScaleRegs::kEntries; // number of input/output slots in scaler
constexpr std::size_t AccScaleRegs::kWidth;   // number of data elements in each packet being handled by a slot

AccScaleRegs::AccScaleRegs(std::string /*name*/, IMPL_CTOR) {
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    regs_val_Q_[slot]  <= regs_val_D_[slot];
    regs_bits_Q_[slot] <= regs_bits_D_[slot];
    out_regs_Q_[slot]  <= out_regs_D_[slot];
  }
  UPDATE(updateOutput).reads(head_oh, out_regs_Q_).writes(out_bits);
  UPDATE(updateRegs).reads(req_fire, req_bits, tail_oh, out_fire, head_oh, out_regs_Q_)
                    .writes(regs_val_D_, regs_bits_D_, out_regs_D_);
}

// Update output regs when they release head slot. Update input regs when they accept tail slot. 
// Copy some metadata to output regs.
void AccScaleRegs::updateRegs() {
  const auto head = static_cast<std::uint8_t>(*head_oh); // which slot supplies o/p
  const auto tail = static_cast<std::uint8_t>(*tail_oh); // which slot accepts next i/p
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    if (out_fire == 1 && (head & (1u << slot)) != 0) {   // current head slot is leaving?
      regs_val_D_[slot] = 0;                             // if yes, mark it empty for the next input
      trace(acc_scale_regs_, "release slot=%u\n", static_cast<unsigned>(slot));
    }
    if (req_fire == 1 && (tail & (1u << slot)) != 0) {   // current tail slot is accepting input?
      regs_bits_D_[slot] = *req_bits;                    // if yes, capture the input packet
      regs_val_D_[slot]  = 1;
      auto output        = *out_regs_Q_[slot];
      output.from_dma    = req_bits->norm.acc_read_resp.from_dma;              // copy some metadata
      output.acc_bank_id = u16(req_bits->norm.acc_read_resp.laddr.acc_bank()); // to output slot
      out_regs_D_[slot]  = output;
      trace(acc_scale_regs_, "capture slot=%u elements=%u cmd_id=%u bank=%u from_dma=%u\n",
            static_cast<unsigned>(slot), static_cast<unsigned>(kWidth),
            static_cast<unsigned>(req_bits->norm.acc_read_resp.cmd_id),
            static_cast<unsigned>(output.acc_bank_id), static_cast<unsigned>(output.from_dma));
    }
  }
}

// Drive the final scaled output from the slot selected by head_oh.
void AccScaleRegs::updateOutput() {
  const auto head = static_cast<std::uint8_t>(*head_oh);
  out_bits = AccScaleResp{};
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    if ((head & (1u << slot)) != 0) {
      out_bits = *out_regs_Q_[slot];
    }
  }
}

void AccScaleRegs::reset() {
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    regs_val_Q_[slot].reset(0);
    regs_val_D_[slot].reset(0);
    regs_bits_Q_[slot].reset(AccScaleReq{});
    regs_bits_D_[slot].reset(AccScaleReq{});
    out_regs_Q_[slot].reset(AccScaleResp{});
    out_regs_D_[slot].reset(AccScaleResp{});
  }
  out_bits.reset(AccScaleResp{});
}

} // namespace smesh
