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
  UPDATE(update).reads(acc_resp_in, cmd_in).writes(req_out);
}

void Normalizer::update() {
  if (acc_resp_in.empty() || cmd_in.empty() || req_out.full()) {
    return;
  }

  const auto acc_resp = acc_resp_in.pop();
  const auto cmd = cmd_in.pop();
  assert_always(acc_resp.from_dma != 0, "Normalizer received non-DMA accumulator response");
  assert_always(cmd.laddr.is_acc_addr(), "Normalizer received non-accumulator store command");

  AccNormReq req{};
  req.acc_read_resp = acc_resp;  // from accumulator read response
  req.cmd.len      = cmd.len;                               // from write_norm_queue_
  req.cmd.stats_id = cmd.acc_norm_stats_id;                 // from write_norm_queue_
  req.cmd.cmd      = static_cast<u8>(cmd.laddr.norm_cmd()); // from write_norm_queue_
  req_out.push(req);

  trace("normalizer: accepted acc_laddr=0x%x len=%u stats_id=%u norm_cmd=%u cmd_id=%u",
        static_cast<unsigned>(acc_resp.laddr.raw),
        static_cast<unsigned>(req.cmd.len),
        static_cast<unsigned>(req.cmd.stats_id),
        static_cast<unsigned>(req.cmd.cmd),
        static_cast<unsigned>(acc_resp.cmd_id));
}

} // namespace smesh
