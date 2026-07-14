// **********************************************************************
// smesh/src/Normalizer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Transform/joins accumulator read data and norm metadata.
*/

#include "Normalizer.hpp"

namespace smesh {

Normalizer::Normalizer(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(req_rdy);
  UPDATE(updateRespView).writes(resp_val, resp_bits);
  UPDATE(updateRespPop).reads(resp_rdy);
  UPDATE(update).reads(req_val, req_bits);
}

void Normalizer::updateReady() {
  req_rdy = bit(!resp_valid_);
}

void Normalizer::updateRespView() {
  resp_val = bit(resp_valid_);
  resp_bits = resp_valid_ ? resp_entry_ : AccNormReq{};
}

void Normalizer::updateRespPop() {
  if (resp_valid_ && resp_rdy != 0) {
    resp_valid_ = false;
    resp_entry_ = AccNormReq{};
  }
}

void Normalizer::update() {
  if (req_val == 0 || resp_valid_) {
    return;
  }

  const auto req = *req_bits;
  assert_always(req.acc_read_resp.from_dma != 0, "Normalizer received non-DMA accumulator response");
  resp_entry_ = req;
  resp_valid_ = true;

  trace("normalizer: accepted acc_laddr=0x%x len=%u stats_id=%u norm_cmd=%u cmd_id=%u",
        static_cast<unsigned>(req.acc_read_resp.laddr.raw),
        static_cast<unsigned>(req.cmd.len),
        static_cast<unsigned>(req.cmd.stats_id),
        static_cast<unsigned>(req.cmd.cmd),
        static_cast<unsigned>(req.acc_read_resp.cmd_id));
}

} // namespace smesh
