// **********************************************************************
// smesh/src/accum_response/NormReciprocal.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormReciprocal.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

namespace smesh {

NormReciprocal::NormReciprocal(std::string /*name*/, IMPL_CTOR) {
  pending_Q_ <= pending_D_;
  UPDATE(updateStart).reads(slot_states, stats, pending_Q_)
                     .writes(started, start_id, pending_D_);
  UPDATE(updateResult).reads(pending_Q_).writes(finished, finish_id, result);
}

void NormReciprocal::updateStart() {
  started  = 0;
  start_id = 0;
  if (pending_Q_->valid == 1) {
    pending_D_ = NormReciprocalPending{};
    return;
  }

  const auto states = *slot_states;
  const auto values = *stats;
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state       = static_cast<NormFsmState>(static_cast<std::uint8_t>(states.state[id]));
    if (state != NormFsmState::GetInvStddev && state != NormFsmState::GetInvSumExp) continue;
    float reciprocal = 0;
    if (state == NormFsmState::GetInvSumExp) {
      reciprocal = 127.0f / static_cast<float>(static_cast<std::int32_t>(static_cast<std::uint32_t>(values.sum[id])));
    } else {
      const auto stddev = values.stddev[id];
      assert(stddev > 0);
      reciprocal = 1.0f / static_cast<float>(stddev);
    }
    std::uint32_t bits     = 0;
    static_assert(sizeof(bits) == sizeof(reciprocal), "binary32 requires 32-bit float");
    static_assert(std::numeric_limits<float>::is_iec559, "binary32 requires IEEE float");
    std::memcpy(&bits, &reciprocal, sizeof(bits));
    pending_D_             = NormReciprocalPending{1, u8(id), u32(bits)};
    started                = 1;
    start_id               = u8(id);
    return;
  }
}

void NormReciprocal::updateResult() {
  const auto pending = *pending_Q_;
  finished           = pending.valid;
  finish_id          = pending.slot;
  result             = pending.value;
}

void NormReciprocal::reset() {
  pending_Q_.reset(NormReciprocalPending{});
  pending_D_.reset(NormReciprocalPending{});
  started.reset(0);
  start_id.reset(0);
  finished.reset(0);
  finish_id.reset(0);
  result.reset(0);
}

} // namespace smesh
