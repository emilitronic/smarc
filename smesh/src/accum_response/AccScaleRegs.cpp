// **********************************************************************
// smesh/src/accum_response/AccScaleRegs.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026

#include "AccScaleRegs.hpp"

TraceKey(acc_scale_regs_);

namespace smesh {

constexpr std::size_t AccScaleRegs::kEntries; // number of input/output slots in scaler
constexpr std::size_t AccScaleRegs::kWidth;   // number of data elements in each packet being handled by a slot
constexpr std::size_t AccScaleRegs::kReturnLanes;

AccScaleRegs::AccScaleRegs(std::string /*name*/, IMPL_CTOR) {
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    regs_val_Q_[slot]        <= regs_val_D_[slot];
    regs_bits_Q_[slot]       <= regs_bits_D_[slot];
    out_regs_Q_[slot]        <= out_regs_D_[slot];
    completed_masks_Q_[slot] <= completed_masks_D_[slot];
  }
  UPDATE(updateOutput).reads(head_oh, out_regs_Q_).writes(out_bits);
  UPDATE(updateRegs).reads(req_fire, req_bits, tail_oh, out_fire, head_oh, out_regs_Q_)
                    .reads(regs_val_Q_, completed_masks_Q_, result_val, result_bits)
                    .writes(regs_val_D_, regs_bits_D_, out_regs_D_, completed_masks_D_);
}

// Update output regs when they release head slot. Update input regs when they accept tail slot. 
// Copy some metadata to output regs.
// Returning lane results write their named output element and mark it complete.
void AccScaleRegs::updateRegs() {
  const auto head = static_cast<std::uint8_t>(*head_oh); // which slot supplies o/p
  const auto tail = static_cast<std::uint8_t>(*tail_oh); // which slot accepts next i/p

  // simulation correctness check
  for (std::size_t lane = 0; lane < kReturnLanes; ++lane) {
    if (result_val[lane] == 1) {
      const auto result = *result_bits[lane];
      assert_always(result.slot < kEntries && result.element < kWidth, "AccScale result names an invalid output slot or element");
      // Original's fixed return connections for the current four-lane group.
      const auto flat = static_cast<unsigned>(result.slot) * kWidth + static_cast<unsigned>(result.element);
      assert_always(flat % kReturnLanes == lane, "AccScale result names an element not connected to this lane");
    }
  }

  // write ea. returned element into corret out_regs and mark it complete; 
  // also update input/output regs for accepted/released slots
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    auto output         = *out_regs_Q_[slot];
    auto completed      = *completed_masks_Q_[slot];
    bool output_changed = false;
    // If current head slot is leaving...
    if (out_fire == 1 && (head & (1u << slot)) != 0) {   
      regs_val_D_[slot] = 0;                             // ...mark it empty for the next input
      trace(acc_scale_regs_, "release slot=%u\n", static_cast<unsigned>(slot));
    }
    // If current tail slot is accepting input...
    if (req_fire == 1 && (tail & (1u << slot)) != 0) {   
      regs_bits_D_[slot] = *req_bits;                    // ...capture the input packet
      regs_val_D_[slot]  = 1;
      output.from_dma    = req_bits->norm.acc_read_resp.from_dma;              // copy some metadata
      output.acc_bank_id = u16(req_bits->norm.acc_read_resp.laddr.acc_bank()); // to output slot
      completed          = CompletedMask{}; // new packet must produce every output element again
      output_changed     = true;
      trace(acc_scale_regs_, "capture slot=%u elements=%u cmd_id=%u bank=%u from_dma=%u\n",
            static_cast<unsigned>(slot), static_cast<unsigned>(kWidth),
            static_cast<unsigned>(req_bits->norm.acc_read_resp.cmd_id),
            static_cast<unsigned>(output.acc_bank_id), static_cast<unsigned>(output.from_dma));
    }
    // Different lanes can write different elements of the same output row together.
    for (std::size_t lane = 0; lane < kReturnLanes; ++lane) {
      if (result_val[lane] == 0) continue; 
      const auto result = *result_bits[lane];                          // a valid lane result...
      if (result.slot != slot) continue;
      const auto element = static_cast<std::uint16_t>(result.element); // ...intended for this output slot

      assert_always(regs_val_Q_[slot] == 1, "AccScale result returned to an empty input slot");
      assert_always(!(req_fire == 1 && (tail & (1u << slot)) != 0), "AccScale result returned while replacing its input packet");
      assert_always(completed[element] == 0, "AccScale output element returned more than once");
      // copy valid result into the matching  element position
      output.full_data[element] = result.full_data;
      output.data[element]      = result.data;
      completed[element]        = 1;
      output_changed            = true;
      trace(acc_scale_regs_, "result lane=%u slot=%u element=%u full=%d data=%d\n",
            static_cast<unsigned>(lane), static_cast<unsigned>(slot),
            static_cast<unsigned>(element), static_cast<int>(result.full_data),
            static_cast<int>(result.data));
    }
    if (output_changed) {
      out_regs_D_[slot] = output;
      completed_masks_D_[slot] = completed;
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
    completed_masks_Q_[slot].reset(CompletedMask{});
    completed_masks_D_[slot].reset(CompletedMask{});
  }
  out_bits.reset(AccScaleResp{});
}

} // namespace smesh
