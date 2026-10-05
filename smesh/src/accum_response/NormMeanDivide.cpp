// **********************************************************************
// smesh/src/accum_response/NormMeanDivide.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormMeanDivide.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace smesh {

NormMeanDivide::NormMeanDivide(std::string /*name*/, IMPL_CTOR) {
  pending_Q_ <= pending_D_;
  UPDATE(updateStart).reads(slot_states, stats, pending_Q_)
                     .writes(started, start_id, pending_D_);
  UPDATE(updateResult).reads(pending_Q_).writes(finished, finish_id, result);
}

void NormMeanDivide::updateStart() {
  started  = 0;
  start_id = 0;
  if (pending_Q_->valid == 1) {
    pending_D_ = NormMeanPending{};
    return;
  }

  const auto states = *slot_states; // encoding of FSM state for each slot
  const auto values = *stats;
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state = static_cast<NormFsmState>(static_cast<std::uint8_t>(states.state[id]));

    // you need to divide for mean or variance, but not for sum or max
    if (state != NormFsmState::GetMean && state != NormFsmState::GetVariance) continue;
    const auto count       = static_cast<std::uint16_t>(values.count[id]);
    assert(count > 0);
    const auto encoded_sum = static_cast<std::uint32_t>(values.sum[id]);
    Acc signed_sum         = 0;
    static_assert(sizeof(signed_sum) == sizeof(encoded_sum), "sum and accumulator widths differ");
    // Sum stores the signed accumulator's wrapped bit pattern.
    std::memcpy(&signed_sum, &encoded_sum, sizeof(signed_sum));
    pending_D_ = NormMeanPending{1, u8(id), static_cast<Acc>(signed_sum / count)};
    started    = 1;
    start_id   = u8(id);
    return;
  }
}

void NormMeanDivide::updateResult() {
  const auto pending = *pending_Q_;
  finished           = pending.valid;
  finish_id          = pending.slot;
  result             = pending.value;
}

void NormMeanDivide::reset() {
  pending_Q_.reset(NormMeanPending{});
  pending_D_.reset(NormMeanPending{});
  started.reset(0);
  start_id.reset(0);
  finished.reset(0);
  finish_id.reset(0);
  result.reset(0);
}

} // namespace smesh
