// **********************************************************************
// smesh/src/accum_response/AccScaleWorkClass.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026

#include "AccScaleWorkClass.hpp"

namespace smesh {

AccScaleWorkClass::AccScaleWorkClass(std::string /*name*/, unsigned total_lanes,
                                   unsigned normalization_lanes, IMPL_CTOR) {
  static_assert(kEntries == 3, "classification ports describe three row slots");
  assert_always(total_lanes > 0 && normalization_lanes <= total_lanes,
                "normalization lane count must not exceed positive total lane count");

  // Build Original's lookup table at construction, not during simulated cycles.
  // Integer ratios implement its comparison and positive half-up rounding.
  const auto total = static_cast<std::uint64_t>(total_lanes);
  const auto norm = static_cast<std::uint64_t>(normalization_lanes);
  const auto target = (2 * kEntries * norm + total) / (2 * total);
  for (unsigned mask = 0; mask < static_assignment_policy_.size(); ++mask) {
    unsigned count = 0;
    for (std::size_t slot = 0; slot < kEntries; ++slot) count += (mask >> slot) & 1u;
    unsigned policy = mask;
    if (normalization_lanes == 0) {
      policy = 0;
    } else if (count * total < kEntries * norm) {
      // Original scans the binary string from its most significant bit.
      for (std::size_t slot = kEntries; slot > 0 && count < target; --slot) {
        const auto bit_mask = 1u << (slot - 1);
        if ((policy & bit_mask) == 0) {
          policy |= bit_mask;
          ++count;
        }
      }
    }
    static_assignment_policy_[mask] = u3(policy);
  }

  UPDATE(updateClassification).reads(regs_val, regs_bits).writes(norm_mask, current_policy);
}

// Inspect each saved row's activation, then select its lane-group policy.
void AccScaleWorkClass::updateClassification() {
  unsigned mask = 0;
  for (std::size_t slot = 0; slot < kEntries; ++slot) {
    const auto act = static_cast<std::uint8_t>(regs_bits[slot]->norm.acc_read_resp.act);
    if (regs_val[slot] == 1 && (act == 2 || act == 3 || act == 4)) {
      mask |= 1u << slot;
    }
  }
  norm_mask = u3(mask);
  current_policy = static_assignment_policy_[mask];
}

void AccScaleWorkClass::reset() {
  norm_mask.reset(0);
  current_policy.reset(static_assignment_policy_[0]);
}

} // namespace smesh
