// **********************************************************************
// smesh/src/StScaleCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-path scale-stage control implementation.
*/

#include "StScaleCtrl.hpp"

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
  const auto req = *scale_deq_bits;
  const auto norm = *normalizer_resp_bits;
  const auto laddr = req.laddr;

  const bool bypass_acc_scale = laddr.is_garbage() || !laddr.is_acc_addr(); // don't scale if spad addr or garbage
  const bool acc_waiting_to_be_scaled = // scale if
      scale_deq_val != 0 &&             // scale val=1
      !laddr.is_garbage() &&            // not garbage
      laddr.is_acc_addr() &&            // is accumulator address
      issue_enq_rdy != 0;               // issue rdy=1

  bool next_scale_deq_rdy       = false;
  bool next_normalizer_resp_rdy = false;
  bool next_acc_scale_req_val   = false;
  bool next_issue_enq_val       = false;
  AccScaleReq next_acc_scale_req{};
  next_acc_scale_req.norm = norm;

  // CASE 1: spad or garbage, bypass accumulator scaling.
  if (bypass_acc_scale) {
    next_scale_deq_rdy = issue_enq_rdy != 0; // scale rdy <= issue rdy
    next_issue_enq_val = scale_deq_val != 0; // scale val => issue val
  }
  // CASE 2: accumulator, so normalizer->AccScaleUnit and scale_q->issue_q move together (enforces a paired transfer)
  else {
    // do norm --> acc transfer if acc rdy + norm val & scale --> issue transfer *can* be made
    next_normalizer_resp_rdy = acc_waiting_to_be_scaled && acc_scale_req_rdy != 0;   // norm rdy <= acc rdy (scale val=1, issue rdy=1) 
    next_acc_scale_req_val   = acc_waiting_to_be_scaled && normalizer_resp_val != 0; // norm val (scale val=1, issue rdy=1) => acc val
    // do scale --> issue transfer if issue rdy + scale val &  norm --> acc transfer *can* be made
    next_scale_deq_rdy = normalizer_resp_val != 0 && acc_scale_req_rdy != 0 && issue_enq_rdy != 0; // scale rdy <= issue rdy (norm val, acc rdy)
    next_issue_enq_val = normalizer_resp_val != 0 && acc_scale_req_rdy != 0 && scale_deq_val != 0; // scale val (norm val, acc rdy) => issue val
  }

  scale_deq_rdy       = bit(next_scale_deq_rdy);
  normalizer_resp_rdy = bit(next_normalizer_resp_rdy);
  acc_scale_req_val   = bit(next_acc_scale_req_val);
  acc_scale_req_bits  = next_acc_scale_req;
  issue_enq_val       = bit(next_issue_enq_val);
}

} // namespace smesh
