// **********************************************************************
// smesh/src/mvout/StScaleCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-path scale-stage control implementation.
*/

#include "StScaleCtrl.hpp"

TraceKey(st_scale_view);

namespace smesh {

StScaleCtrl::StScaleCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(scale_deq_val,
                       scale_deq_bits,
                       normalizer_resp_val,
                       normalizer_resp_bits,
                       acc_scale_req_rdy,
                       issue_enq_rdy)
                .writes(scale_deq_rdy,
                        normalizer_resp_rdy,
                        acc_scale_req_val,
                        acc_scale_req_bits,
                        issue_enq_val);
}

void StScaleCtrl::update() {
  const auto norm           = *normalizer_resp_bits; // data
  const auto req            = *scale_deq_bits;       // metadata
  const auto laddr          = req.laddr;
  const bool bypass_store   = scale_deq_val != 0 && (laddr.is_garbage() || !laddr.is_acc_addr());
  const bool ex_response    = normalizer_resp_val != 0 && norm.acc_read_resp.from_dma == 0;
  const bool store_response = normalizer_resp_val != 0 && norm.acc_read_resp.from_dma != 0;
  const bool store_accum    = scale_deq_val != 0 && !bypass_store && store_response && issue_enq_rdy != 0;

  // ExCtrl data needs no Store metadata. Store data and metadata advance together.
  normalizer_resp_rdy = bit(ex_response ? acc_scale_req_rdy != 0 : store_accum && acc_scale_req_rdy != 0);
  acc_scale_req_val   = bit(ex_response || store_accum);
  AccScaleReq scale_req{};
  scale_req.norm      = norm;
  acc_scale_req_bits  = scale_req;

  scale_deq_rdy = bit(bypass_store ? issue_enq_rdy != 0 : store_accum && acc_scale_req_rdy != 0);
  issue_enq_val = bit(bypass_store || (store_accum && acc_scale_req_rdy != 0));
  if ((scale_deq_val == 1 && scale_deq_rdy == 1) || (normalizer_resp_val == 1 && normalizer_resp_rdy == 1)) {
    trace(st_scale_view, "scale=%u norm=%u dma=%u srdy=%u nrdy=%u aval=%u ardy=%u issue=%u\n",
          static_cast<unsigned>(scale_deq_val), static_cast<unsigned>(normalizer_resp_val),
          static_cast<unsigned>(norm.acc_read_resp.from_dma),
          static_cast<unsigned>(scale_deq_rdy), static_cast<unsigned>(normalizer_resp_rdy),
          static_cast<unsigned>(acc_scale_req_val), static_cast<unsigned>(acc_scale_req_rdy),
          static_cast<unsigned>(issue_enq_val));
  }
}

} // namespace smesh
