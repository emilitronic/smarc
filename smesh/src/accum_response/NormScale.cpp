// **********************************************************************
// smesh/src/accum_response/NormScale.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormScale.hpp"

#include <cstdint>
#include <cstring>
#include <limits>

namespace smesh {
namespace {

float fromBits(u32 bits) {
  const auto raw = static_cast<std::uint32_t>(bits);
  float value = 0;
  static_assert(sizeof(raw) == sizeof(value), "binary32 requires 32-bit float");
  static_assert(std::numeric_limits<float>::is_iec559, "binary32 requires IEEE float");
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

u32 toBits(float value) {
  std::uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  return u32(raw);
}

} // namespace

NormScale::NormScale(std::string /*name*/, IMPL_CTOR) {
  pending_Q_ <= pending_D_;
  UPDATE(updateStart).reads(slot_states, stats, saved, pending_Q_)
                     .writes(started, start_id, pending_D_);
  UPDATE(updateResult).reads(pending_Q_).writes(finished, finish_id, result);
}

void NormScale::updateStart() {
  started  = 0;
  start_id = 0;
  if (pending_Q_->valid == 1) {
    pending_D_ = NormScalePending{};
    return;
  }

  const auto states  = *slot_states;
  const auto values  = *stats;
  const auto packets = *saved;
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state = static_cast<NormFsmState>(static_cast<std::uint8_t>(states.state[id]));
    if (state != NormFsmState::GetScaledInvStddev && state != NormFsmState::GetScaledInvSumExp) continue;
    const auto factor = fromBits(state == NormFsmState::GetScaledInvSumExp ? values.inv_sum_exp[id] : values.inv_stddev[id]);
    const auto scale  = fromBits(packets.packet[id].acc_read_resp.scale);
    pending_D_        = NormScalePending{1, u8(id), toBits(factor * scale)};
    started           = 1;
    start_id          = u8(id);
    return;
  }
}

void NormScale::updateResult() {
  const auto pending = *pending_Q_;
  finished           = pending.valid;
  finish_id          = pending.slot;
  result             = pending.value;
}

void NormScale::reset() {
  pending_Q_.reset(NormScalePending{});
  pending_D_.reset(NormScalePending{});
  started.reset(0);
  start_id.reset(0);
  finished.reset(0);
  finish_id.reset(0);
  result.reset(0);
}

} // namespace smesh
