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
  UPDATE(update).reads(req_val, req_bits).writes(resp_out);
}

void AccScaleUnit::updateReady() {
  req_rdy = bit(!resp_out.full());
}

void AccScaleUnit::update() {
  if (req_val == 0 || resp_out.full()) {
    return;
  }

  const auto req = *req_bits;
  const auto& acc = req.norm.acc_read_resp;
  AccScaleResp resp{};
  resp.full_data = acc.data;
  resp.data = acc.data;
  resp.acc_bank_id = static_cast<u16>(acc.laddr.acc_bank());
  resp.from_dma = acc.from_dma;
  resp_out.push(resp);

  trace("acc_scale_unit: accepted acc_laddr=0x%x bank=%u len=%u cmd_id=%u",
        static_cast<unsigned>(acc.laddr.raw),
        static_cast<unsigned>(resp.acc_bank_id),
        static_cast<unsigned>(acc.len),
        static_cast<unsigned>(acc.cmd_id));
}

} // namespace smesh
