// **********************************************************************
// smesh/src/accum_response/AccScaleLane.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026

#include "AccScaleLane.hpp"

TraceKey(acc_scale_lane_);

namespace smesh {

constexpr std::size_t AccScaleLane::kEntries;
constexpr std::size_t AccScaleLane::kWidth;

AccScaleLane::AccScaleLane(std::string /*name*/, unsigned lane_index, unsigned group_lanes, bool normalization_lane, IMPL_CTOR)
    : lane_index_(lane_index), group_lanes_(group_lanes), normalization_lane_(normalization_lane) {
  assert_always(group_lanes > 0 && lane_index < group_lanes && lane_index < kEntries * kWidth, "Invalid AccScaleLane connection map");
  last_grant_Q_   <= last_grant_D_;
  arb_out_val_Q_  <= arb_out_val_D_;
  arb_out_bits_Q_ <= arb_out_bits_D_;
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    fired_masks_Q_[slot] <= fired_masks_D_[slot];
  }
  UPDATE(updateArbiter).reads(regs_val, regs_bits, current_policy, fired_masks_Q_, last_grant_Q_)
                       .writes(arb_val, arb_bits);
  UPDATE(updateRegisters).reads(arb_val, arb_bits, req_fire, tail_oh, fired_masks_Q_)
                         .writes(arb_out_val_D_, arb_out_bits_D_, last_grant_D_, fired_masks_D_);
}

// Select one connected element that has not already entered this lane.
void AccScaleLane::updateArbiter() {
  arb_val              = 0;
  arb_bits             = AccScaleElem{};
  constexpr auto total = kEntries * kWidth;
  const auto last      = static_cast<std::uint16_t>(*last_grant_Q_);
  const auto policy    = static_cast<std::uint8_t>(*current_policy);
  // These bounded checks model fixed arbiter inputs in flattened slot/element order.
  // Round-robin priority starts after the previously accepted candidate and wraps.
  for (std::size_t offset = 1; offset <= total; ++offset) {
    const auto flat             = (last + offset) % total;
    if (flat % group_lanes_ != lane_index_) continue;
    const auto slot             = flat / kWidth;
    const auto element          = flat % kWidth;
    const bool assigned_to_norm = (policy & (1u << slot)) != 0;
    if (regs_val[slot] == 0 || assigned_to_norm != normalization_lane_ ||
        (*fired_masks_Q_[slot])[element] == 1) continue;

    const auto packet  = *regs_bits[slot];
    const auto& row    = packet.norm.acc_read_resp;
    AccScaleElem selected{};
    selected.data      = row.data[element];
    selected.full_data = row.data[element];
    selected.scale     = row.scale;
    selected.act       = row.act;
    if (normalization_lane_) {
      selected.igelu_qb      = row.igelu_qb;
      selected.igelu_qc      = row.igelu_qc;
      selected.iexp_qln2     = row.iexp_qln2;
      selected.iexp_qln2_inv = row.iexp_qln2_inv;
      selected.mean          = packet.norm.mean;
      selected.max           = packet.norm.max;
      selected.inv_stddev    = packet.norm.inv_stddev;
      selected.inv_sum_exp   = packet.norm.inv_sum_exp;
    }
    selected.slot    = u8(slot);
    selected.element = u16(element);
    arb_bits         = selected;
    arb_val          = 1;
    break;
  }
}

// Register the accepted element and mark it sent; clear masks for new input packets.
void AccScaleLane::updateRegisters() {
  arb_out_val_D_ = *arb_val; // arbOut is replaced each cycle, including invalid cycles
  const bool accepted = arb_val == 1; // Original arbiter ready is tied high
  const auto selected = *arb_bits;
  if (accepted) {
    arb_out_bits_D_ = selected;
    last_grant_D_   = u16(static_cast<unsigned>(selected.slot) * kWidth + static_cast<unsigned>(selected.element));
    trace(acc_scale_lane_, "accept slot=%u element=%u value=%d\n",
          static_cast<unsigned>(selected.slot), static_cast<unsigned>(selected.element),
          static_cast<int>(selected.data));
  }
  const auto tail = static_cast<std::uint8_t>(*tail_oh);
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    auto fired   = *fired_masks_Q_[slot];
    bool changed = false;
    if (req_fire == 1 && (tail & (1u << slot)) != 0) {
      fired   = FiredMask{};
      changed = true;
    }
    if (accepted && selected.slot == slot) {
      assert_always(!(req_fire == 1 && (tail & (1u << slot)) != 0), "Cannot dispatch an old element while replacing its input packet");
      fired[static_cast<std::uint16_t>(selected.element)] = 1;
      changed = true;
    }
    if (changed) fired_masks_D_[slot] = fired;
  }
}

void AccScaleLane::reset() {
  // Original RRArbiter resets its last grant to its first connected candidate.
  last_grant_Q_.reset(u16(lane_index_));
  last_grant_D_.reset(u16(lane_index_));
  arb_val.reset(0);
  arb_bits.reset(AccScaleElem{});
  arb_out_val_Q_.reset(0);
  arb_out_val_D_.reset(0);
  arb_out_bits_Q_.reset(AccScaleElem{});
  arb_out_bits_D_.reset(AccScaleElem{});
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    fired_masks_Q_[slot].reset(FiredMask{});
    fired_masks_D_[slot].reset(FiredMask{});
  }
}

} // namespace smesh
