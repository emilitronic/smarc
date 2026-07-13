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
  UPDATE(update).reads(req_val, req_bits).writes(req_out);
}

void Normalizer::updateReady() {
  req_rdy = bit(!req_out.full());
}

void Normalizer::update() {
  if (req_val == 0 || req_out.full()) {
    return;
  }

  const auto req = *req_bits;
  assert_always(req.acc_read_resp.from_dma != 0, "Normalizer received non-DMA accumulator response");
  req_out.push(req);

  trace("normalizer: accepted acc_laddr=0x%x len=%u stats_id=%u norm_cmd=%u cmd_id=%u",
        static_cast<unsigned>(req.acc_read_resp.laddr.raw),
        static_cast<unsigned>(req.cmd.len),
        static_cast<unsigned>(req.cmd.stats_id),
        static_cast<unsigned>(req.cmd.cmd),
        static_cast<unsigned>(req.acc_read_resp.cmd_id));
}

} // namespace smesh
