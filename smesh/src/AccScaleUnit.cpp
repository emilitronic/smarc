// **********************************************************************
// smesh/src/AccScaleUnit.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Accumulator scale-stage skeleton implementation.
*/

#include "AccScaleUnit.hpp"

namespace smesh {

AccScaleUnit::AccScaleUnit(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(req_rdy);
  UPDATE(updateOutView).writes(out_val, out_bits);
  UPDATE(updateOutPop).reads(out_rdy);
  UPDATE(update).reads(req_val, req_bits);
}

void AccScaleUnit::updateReady() {
  req_rdy = bit(!out_valid_);
}

void AccScaleUnit::updateOutView() {
  out_val = bit(out_valid_);
  out_bits = out_valid_ ? out_entry_ : AccScaleResp{};
}

void AccScaleUnit::updateOutPop() {
  if (out_valid_ && out_rdy != 0) {
    out_valid_ = false;
    out_entry_ = AccScaleResp{};
  }
}

void AccScaleUnit::update() {
  if (req_val == 0 || out_valid_) {
    return;
  }

  const auto req = *req_bits;
  const auto& acc = req.norm.acc_read_resp;
  AccScaleResp resp{};
  resp.full_data = acc.data;
  resp.data = acc.data;
  resp.acc_bank_id = static_cast<u16>(acc.laddr.acc_bank());
  resp.from_dma = acc.from_dma;
  out_entry_ = resp;
  out_valid_ = true;

  trace("acc_scale_unit: accepted acc_laddr=0x%x bank=%u len=%u cmd_id=%u",
        static_cast<unsigned>(acc.laddr.raw),
        static_cast<unsigned>(resp.acc_bank_id),
        static_cast<unsigned>(acc.len),
        static_cast<unsigned>(acc.cmd_id));
}

} // namespace smesh
