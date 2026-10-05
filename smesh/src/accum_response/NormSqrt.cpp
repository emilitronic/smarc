// **********************************************************************
// smesh/src/accum_response/NormSqrt.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormSqrt.hpp"

#include <cstdint>

namespace smesh {
namespace {

std::uint32_t integerSqrt(std::uint32_t value) {
  std::uint32_t root = 0;
  std::uint32_t bit = 1u << 30;
  while (bit > value) bit >>= 2;
  while (bit != 0) {
    if (value >= root + bit) {
      value -= root + bit;
      root = (root >> 1) + bit;
    } else {
      root >>= 1;
    }
    bit >>= 2;
  }
  return root;
}

} // namespace

NormSqrt::NormSqrt(std::string /*name*/, IMPL_CTOR) {
  pending_Q_ <= pending_D_;
  UPDATE(updateStart).reads(slot_states, stats, pending_Q_)
                     .writes(started, start_id, pending_D_);
  UPDATE(updateResult).reads(pending_Q_).writes(finished, finish_id, result);
}

void NormSqrt::updateStart() {
  started  = 0;
  start_id = 0;
  if (pending_Q_->valid == 1) {
    pending_D_ = NormSqrtPending{};
    return;
  }

  const auto states = *slot_states;
  const auto values = *stats;
  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state = static_cast<NormFsmState>(static_cast<std::uint8_t>(states.state[id]));
    if (state != NormFsmState::GetStddev) continue;
    const auto variance = static_cast<std::uint32_t>(values.variance[id]);
    pending_D_ = NormSqrtPending{1, u8(id), static_cast<Acc>(integerSqrt(variance))};
    started = 1;
    start_id = u8(id);
    return;
  }
}

void NormSqrt::updateResult() {
  const auto pending = *pending_Q_;
  finished           = pending.valid;
  finish_id          = pending.slot;
  result             = pending.value;
}

void NormSqrt::reset() {
  pending_Q_.reset(NormSqrtPending{});
  pending_D_.reset(NormSqrtPending{});
  started.reset(0);
  start_id.reset(0);
  finished.reset(0);
  finish_id.reset(0);
  result.reset(0);
}

} // namespace smesh
