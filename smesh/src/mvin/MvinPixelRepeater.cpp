// **********************************************************************
// smesh/src/mvin/MvinPixelRepeater.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path pixel repetition stage implementation.
*/

#include "MvinPixelRepeater.hpp"

namespace smesh {

MvinPixelRepeater::MvinPixelRepeater(std::string /*name*/, bool passthrough, IMPL_CTOR)
    : passthrough_(passthrough) {
  entry_Q_ <= entry_D_;
  if (passthrough_) {
    UPDATE(updateView).reads(in_val, in_bits).writes(out_val, out_bits, underflow_);
    UPDATE(updateReady).reads(out_rdy).writes(in_rdy);
  } else {
    UPDATE(updateView).reads(entry_Q_).writes(out_val, out_bits, underflow_);
    UPDATE(updateReady).reads(entry_Q_, out_rdy).writes(in_rdy);
    UPDATE(updateStorage).reads(entry_Q_, in_val, in_bits, in_rdy,
                                out_val, out_bits, out_rdy, underflow_).writes(entry_D_);
  }
}

// Build the next SPAD write from the saved pixel data and its current shift.
void MvinPixelRepeater::updateView() {
  underflow_ = 0;
  if (passthrough_) {
    if (in_val == 1) {
      assert_always(in_bits->pixel_repeats == 1,
                    "MvinPixelRepeater passthrough requires pixel_repeats=1");
    }
    out_val = in_val;
    out_bits = in_val == 1 ? *in_bits : DmaReadResp{};
    return;
  }

  const auto entry = *entry_Q_;
  out_val = 0;
  out_bits = DmaReadResp{};
  if (entry.valid == 0) return;
  auto row = entry.bits;
  const unsigned remaining = static_cast<std::uint8_t>(entry.remaining);
  bool underflow = false;
  row.last = bit(entry.bits.last == 1 && remaining == 0);
  if (!row.laddr.is_acc_addr()) {
    const auto address = row.laddr.full_sp_addr();
    const auto floor = address < kSpRows / 2 ? 0u : static_cast<unsigned>(kSpRows / 2);
    underflow = address < floor + remaining;
    const auto destination = underflow ? floor : address - remaining;
    row.laddr.raw = (row.laddr.raw & ~kLocalAddrDataMask) | destination;

    // Original truncates out_shift to log2Up(block_cols / 2 + 1) bits.
    constexpr auto shift_mask = lowBitMask(log2Up(kDim / 2 + 1));
    const auto shift = (remaining * static_cast<unsigned>(row.len)) & shift_mask;
    const auto byte_shift = shift * sizeof(Elem);
    row.data = {};
    // This bounded loop describes a shift of the fixed-width input row.
    for (std::size_t byte = 0; byte < kDim * sizeof(Elem); ++byte) {
      if (byte >= byte_shift) row.data[byte] = entry.bits.data[byte - byte_shift];
    }
    constexpr auto lane_mask = (std::uint64_t{1} << kDim) - 1;
    row.mask = static_cast<std::uint32_t>(
        (std::uint64_t{static_cast<std::uint32_t>(entry.bits.mask)} << shift) & lane_mask);
  }
  underflow_ = bit(underflow);
  out_val = bit(!underflow);
  out_bits = row;
}

// Reuse the saved-packet register when the final copy is accepted this cycle.
void MvinPixelRepeater::updateReady() {
  in_rdy = passthrough_ ? *out_rdy
                       : bit(entry_Q_->valid == 0 ||
                             (out_rdy == 1 && entry_Q_->remaining == 0));
}

// Skip copies below the SPAD-half boundary even if the consumer is stalled.
void MvinPixelRepeater::updateStorage() {
  auto next = *entry_Q_;
  const bool advance = next.valid == 1 &&
                       (underflow_ == 1 || (out_val == 1 && out_rdy == 1));
  const bool push = in_val == 1 && in_rdy == 1;
  if (!advance && !push) return;
  if (advance) {
    trace("mvin_pixel_repeater: %s addr=0x%x mask=0x%x remaining=%u last=%u\n",
          underflow_ == 1 ? "skip" : "write", unsigned(out_bits->laddr.raw),
          unsigned(out_bits->mask), unsigned(next.remaining), unsigned(out_bits->last));
    if (next.remaining == 0) next = Entry{};
    else next.remaining = u8(static_cast<std::uint8_t>(next.remaining) - 1);
  }
  if (push) {
    const auto row = *in_bits;
    assert_always(row.pixel_repeats > 0,
                  "MvinPixelRepeater requires a positive pixel repeat count");
    assert_always(!row.laddr.is_acc_addr() || row.pixel_repeats == 1,
                  "MvinPixelRepeater cannot repeat accumulator rows");
    next.valid = 1;
    next.bits = row;
    next.remaining = u8(static_cast<std::uint8_t>(row.pixel_repeats) - 1);
    trace("mvin_pixel_repeater: accept addr=0x%x copies=%u cmd_id=%u\n",
          unsigned(row.laddr.raw), unsigned(row.pixel_repeats), unsigned(row.cmd_id));
  }
  entry_D_ = next;
}

void MvinPixelRepeater::reset() {
  entry_D_.reset(Entry{});
  underflow_.reset(0);
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(DmaReadResp{});
}

} // namespace smesh
