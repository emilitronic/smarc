// **********************************************************************
// smesh/src/mvin/MvinScale.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path scaling stage implementation.
*/

#include "MvinScale.hpp"

namespace smesh {
namespace {

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
MvinScale::MvinScale(std::string /*name*/, IMPL_CTOR) {
  entry_Q_ <= entry_D_;
  UPDATE(updateView).reads(entry_Q_).writes(out_val, out_bits);
  UPDATE(updateReady).reads(entry_Q_, out_rdy).writes(in_rdy);
  UPDATE(updateStorage)
      .reads(entry_Q_, in_val, in_bits, in_rdy, out_rdy)
      .writes(entry_D_);
}

void MvinScale::updateView() {
  const auto entry = *entry_Q_;
  out_val  = entry.valid;
  out_bits = entry.valid == 1 ? repeatedRow(entry) : DmaReadResp{}; //
}

// Accept a new row when empty, or when the current row's final copy is accepted.
void MvinScale::updateReady() {
  const auto entry = *entry_Q_;
  in_rdy = bit(entry.valid == 0 || (entry.remaining == 0 && out_rdy == 1));
}

// Pushing load-returns in, popping load-returns out (of 1 register)
void MvinScale::updateStorage() {
  const auto current = *entry_Q_;
  const bool pop  = current.valid == 1 && out_rdy == 1;
  const bool push = in_val == 1 && in_rdy == 1;
  if (!pop && !push) return;

  if (pop) {
    trace("mvin_scale: accepted row cmd_id=%u offset=%u last=%u",
          static_cast<unsigned>(current.bits.cmd_id),
          static_cast<unsigned>(current.remaining),
          static_cast<unsigned>(current.remaining == 0 && current.bits.last == 1));
  }
  entry_D_ = advanceRow(current, pop, push, push ? *in_bits : DmaReadResp{});
}

void MvinScale::reset() {
  entry_D_.reset(MvinRepeatEntry{});
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(DmaReadResp{});
}

// ********************* MvinScaleAcc *********************
// Accumulator-width path uses the same repeat sequence.
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
