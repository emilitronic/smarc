// **********************************************************************
// smesh/src/mvin/MvinScale.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path scaling stage implementation.
*/

#include "MvinScale.hpp"
#include "SmeshCommand.hpp"

#include <cmath>
#include <cstring>
#include <limits>

namespace smesh {
namespace {

// ********************* Normal-width Scaling *********************
// Original's normal MVIN scale uses binary32 multiplication and signed int8 output.
// 
std::int8_t scaleElem(std::uint8_t encoded, float scale) {
  const int   value   = encoded <= 127 ? encoded : static_cast<int>(encoded) - 256; // convert to signed int32
  const float product = static_cast<float>(value) * scale; // multiply by configured 32b FP scale
  // Handle 1) NaN (choose limit based on sign bit); 2) clamp ordinary values to signed int8 limit (-128,127)
  if (std::isnan(product)) {return std::signbit(product) ? std::numeric_limits<Elem>::min() : std::numeric_limits<Elem>::max();}
  if (product >= std::numeric_limits<Elem>::max()) return std::numeric_limits<Elem>::max();
  if (product <= std::numeric_limits<Elem>::min()) return std::numeric_limits<Elem>::min();
  // compute round-to-nearest-ties-to-even (banker's rounding) and return as signed int8
  const double lower     = std::floor(static_cast<double>(product)); // get int
  const int rounded_down = static_cast<int>(lower);                 
  const double fraction  = static_cast<double>(product) - lower;     // get fractional part
  const bool round_up    = fraction > 0.5 || (fraction == 0.5 && rounded_down % 2 != 0);
  return static_cast<Elem>(rounded_down + static_cast<int>(round_up));
}

DmaReadResp scaledNormalRow(DmaReadResp row) {
  const auto bits = static_cast<std::uint32_t>(row.scale);
  if (bits == kMvinScaleIdentityBits) return row;
  float scale     = 0;
  static_assert(sizeof(scale) == sizeof(bits) && std::numeric_limits<float>::is_iec559);
  std::memcpy(&scale, &bits, sizeof(scale));
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    row.data[lane] = static_cast<std::uint8_t>(scaleElem(row.data[lane], scale));
  }
  return row;
}

// ********************* Repeat Helpers *********************
// Create the current copy; the countdown sets its destination offset.
DmaReadResp repeatedRow(const MvinRepeatEntry& entry) {
  auto row  = entry.bits;
  row.laddr = row.laddr + static_cast<std::uint16_t>(entry.remaining);
  row.last  = bit(entry.remaining == 0 && row.last == 1);
  return row;
}

// Remove an accepted copy or load a new row and its repeat count.
MvinRepeatEntry advanceRow(MvinRepeatEntry entry, bool pop, bool push, const DmaReadResp& input) {
  if (pop) {
    if (entry.remaining == 0) {
      entry = MvinRepeatEntry{}; // if nothing remains, clear the entry
    } else {
      entry.remaining = u16(static_cast<std::uint16_t>(entry.remaining) - 1); // decrement repeat countdown and keep entry
    }
  }
  if (push) {
    entry.valid     = 1;
    entry.bits      = input;
    entry.remaining = input.repeats; // number of additional copies after the first one
  }
  return entry;
}

} // namespace

// ********************* MvinScale *********************
// Normal-width path. Holds each row until its final repeated copy is accepted.
MvinScale::MvinScale(std::string /*name*/, int latency, IMPL_CTOR)
    : stages_Q_(latency > 0 ? latency - 1 : 0),
      stages_D_(latency > 0 ? latency - 1 : 0) {
  assert_always(latency >= 1, "MVIN scale latency must be at least one cycle");
  entry_Q_ <= entry_D_;
  for (int stage = 0; stage < stages_Q_.size(); ++stage) {
    stages_Q_[stage] <= stages_D_[stage];
  }
  UPDATE(updateView).reads(entry_Q_, stages_Q_).writes(out_val, out_bits);
  UPDATE(updateReady).reads(entry_Q_, stages_Q_, out_rdy).writes(in_rdy);
  UPDATE(updateStorage)
      .reads(entry_Q_, stages_Q_, in_val, in_bits, in_rdy, out_rdy)
      .writes(entry_D_);
  if (stages_Q_.size() > 0) {
    UPDATE(updateStages).reads(entry_Q_, stages_Q_, out_rdy).writes(stages_D_);
  }
}

void MvinScale::updateView() {
  if (stages_Q_.size() == 0) {
    const auto entry = *entry_Q_;
    out_val  = entry.valid;
    out_bits = entry.valid == 1 ? repeatedRow(entry) : DmaReadResp{};
  } else {
    const auto entry = *stages_Q_[stages_Q_.size() - 1];
    out_val  = entry.valid;
    out_bits = entry.valid == 1 ? entry.bits : DmaReadResp{};
  }
}

bool MvinScale::rowAdvanceReady() const {
  bool ready = out_rdy == 1;
  for (int stage = stages_Q_.size(); stage-- > 0;) {
    ready = stages_Q_[stage]->valid == 0 || ready;
  }
  return ready;
}

// Accept a new row when empty, or when the current row's final copy is accepted.
void MvinScale::updateReady() {
  const auto entry = *entry_Q_;
  in_rdy = bit(entry.valid == 0 || (entry.remaining == 0 && rowAdvanceReady()));
}

// Advance one repeated row when the next stage can accept it.
void MvinScale::updateStorage() {
  const auto current = *entry_Q_;
  const bool pop  = current.valid == 1 && rowAdvanceReady();
  const bool push = in_val == 1 && in_rdy == 1;
  if (!pop && !push) return;

  if (pop) {
    trace("mvin_scale: advanced row cmd_id=%u offset=%u last=%u",
          static_cast<unsigned>(current.bits.cmd_id),
          static_cast<unsigned>(current.remaining),
          static_cast<unsigned>(current.remaining == 0 && current.bits.last == 1));
  }
  // What do to with the input row?  Push an incomring row or an empty row if nothing is incoming.
  entry_D_ = advanceRow(current, pop, push, push ? scaledNormalRow(*in_bits) : DmaReadResp{});
}

void MvinScale::updateStages() {
  bool downstream_ready = out_rdy == 1;
  for (int stage = stages_Q_.size(); stage-- > 0;) {
    const auto current = *stages_Q_[stage];
    const bool ready = current.valid == 0 || downstream_ready;
    if (ready) {
      MvinPipeEntry incoming{};
      if (stage == 0) {
        const auto source = *entry_Q_;
        incoming.valid = source.valid;
        if (source.valid == 1) incoming.bits = repeatedRow(source);
      } else {
        incoming = *stages_Q_[stage - 1];
      }
      stages_D_[stage] = incoming;
    }
    downstream_ready = ready;
  }
}

void MvinScale::reset() {
  entry_D_.reset(MvinRepeatEntry{});
  for (int stage = 0; stage < stages_D_.size(); ++stage) {
    stages_D_[stage].reset(MvinPipeEntry{});
  }
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(DmaReadResp{});
}

// ********************* MvinScaleAcc *********************
// Accumulator-width path repeats rows without scaling (Gemmini mvin_scale_acc_args=None).
MvinScaleAcc::MvinScaleAcc(std::string /*name*/, IMPL_CTOR) {
  entry_Q_ <= entry_D_;
  UPDATE(updateView).reads(entry_Q_).writes(out_val, out_bits);
  UPDATE(updateReady).reads(entry_Q_, out_rdy).writes(in_rdy);
  UPDATE(updateStorage)
      .reads(entry_Q_, in_val, in_bits, in_rdy, out_rdy)
      .writes(entry_D_);
}

void MvinScaleAcc::updateView() {
  const auto entry = *entry_Q_;
  out_val  = entry.valid;
  out_bits = entry.valid == 1 ? repeatedRow(entry) : DmaReadResp{};
}

void MvinScaleAcc::updateReady() {
  const auto entry = *entry_Q_;
  in_rdy = bit(entry.valid == 0 || (entry.remaining == 0 && out_rdy == 1));
}

void MvinScaleAcc::updateStorage() {
  const auto current = *entry_Q_;
  const bool pop  = current.valid == 1 && out_rdy == 1;
  const bool push = in_val == 1 && in_rdy == 1;
  if (!pop && !push) return;

  if (pop) {
    trace("mvin_scale_acc: accepted row cmd_id=%u offset=%u last=%u",
          static_cast<unsigned>(current.bits.cmd_id),
          static_cast<unsigned>(current.remaining),
          static_cast<unsigned>(current.remaining == 0 && current.bits.last == 1));
  }
  entry_D_ = advanceRow(current, pop, push, push ? *in_bits : DmaReadResp{});
}

void MvinScaleAcc::reset() {
  entry_D_.reset(MvinRepeatEntry{});
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(DmaReadResp{});
}

// ********************* MvinScaleSplit *********************
// Select a branch from the address type and element width.
MvinScaleSplit::MvinScaleSplit(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateView).reads(in_val, in_bits)
      .writes(normal_val, normal_bits, acc_val, acc_bits);
  UPDATE(updateReady).reads(in_bits, normal_rdy, acc_rdy).writes(in_rdy);
}

void MvinScaleSplit::updateView() {
  const auto data = *in_bits;
  const bool acc_path = data.laddr.is_acc_addr() && data.has_acc_bitwidth == 1;
  normal_val = bit(in_val == 1 && !acc_path);
  normal_bits = in_val == 1 && !acc_path ? data : DmaReadResp{};
  acc_val = bit(in_val == 1 && acc_path);
  acc_bits = in_val == 1 && acc_path ? data : DmaReadResp{};
}

void MvinScaleSplit::updateReady() {
  const auto data = *in_bits;
  const bool acc_path = data.laddr.is_acc_addr() && data.has_acc_bitwidth == 1;
  in_rdy = acc_path ? *acc_rdy : *normal_rdy;
}

void MvinScaleSplit::reset() {
  in_rdy.reset(0);
  normal_val.reset(0);
  normal_bits.reset(DmaReadResp{});
  acc_val.reset(0);
  acc_bits.reset(DmaReadResp{});
}

} // namespace smesh
