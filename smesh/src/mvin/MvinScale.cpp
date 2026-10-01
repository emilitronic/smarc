// **********************************************************************
// smesh/src/mvin/MvinScale.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path scaling stage implementation.
*/

#include "MvinScale.hpp"

namespace smesh {
// scale normal width data coming from DMA reader
MvinScale::MvinScale(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateView).reads(in_val, in_bits).writes(out_val, out_bits);
  UPDATE(updateReady).reads(in_val, in_bits, out_rdy).writes(in_rdy);
}

void MvinScale::updateView() {
  out_val = in_val;
  out_bits = in_val == 1 ? *in_bits : DmaReadResp{};
}

void MvinScale::updateReady() {
  in_rdy = out_rdy;
  if (in_val == 1 && out_rdy == 1) {
    const auto data = *in_bits;
    trace("mvin_scale: identity data cmd_id=%u last=%u",
          static_cast<unsigned>(data.cmd_id), static_cast<unsigned>(data.last));
  }
}

void MvinScale::reset() {
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(DmaReadResp{});
}
// scale accumulator-width data coming from DMA reader
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
  out_val = entry.valid;
  out_bits = entry.valid == 1 ? entry.bits : DmaReadResp{};
}

void MvinScaleAcc::updateReady() {
  const auto entry = *entry_Q_;
  in_rdy = bit(entry.valid == 0 || out_rdy == 1);
}

void MvinScaleAcc::updateStorage() {
  const auto current = *entry_Q_;
  const bool pop = current.valid == 1 && out_rdy == 1;
  const bool push = in_val == 1 && in_rdy == 1;
  if (!pop && !push) return;

  Entry next = current;
  if (pop) {
    trace("mvin_scale_acc: accepted data cmd_id=%u last=%u",
          static_cast<unsigned>(current.bits.cmd_id),
          static_cast<unsigned>(current.bits.last));
    next = Entry{};
  }
  if (push) {
    next.valid = 1;
    next.bits = *in_bits;
  }
  entry_D_ = next;
}

void MvinScaleAcc::reset() {
  entry_D_.reset(Entry{});
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(DmaReadResp{});
}
// split incoming data into normal-width path and accumulator-width path
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
