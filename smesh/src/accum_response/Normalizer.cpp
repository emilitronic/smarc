// **********************************************************************
// smesh/src/accum_response/Normalizer.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Transform/joins accumulator read data and norm metadata.
*/

#include "Normalizer.hpp"

namespace smesh {

Normalizer::Normalizer(std::string /*name*/, IMPL_CTOR) {
  resp_valid_Q_ <= resp_valid_D_;
  resp_entry_Q_ <= resp_entry_D_;
  UPDATE(updateView).reads(resp_valid_Q_, resp_entry_Q_)
                    .writes(resp_val, resp_bits);
  UPDATE(updateReady).reads(resp_valid_Q_, resp_rdy).writes(req_rdy);
  UPDATE(updateBuffer).reads(resp_valid_Q_, resp_entry_Q_, resp_rdy, req_rdy,
                             req_val, req_bits)
                      .writes(resp_valid_D_, resp_entry_D_);
}

void Normalizer::updateView() {
  const bool occupied = *resp_valid_Q_ == 1;
  resp_val            = bit(occupied);
  resp_bits           = occupied ? *resp_entry_Q_ : AccNormReq{};
}

void Normalizer::updateReady() {
  const bool occupied = *resp_valid_Q_ == 1;
  const bool pop      = occupied && resp_rdy == 1;
  req_rdy             = bit(!occupied || pop);
}

void Normalizer::updateBuffer() {
  const bool occupied = *resp_valid_Q_ == 1;
  const bool pop      = occupied && resp_rdy == 1;
  const bool push     = req_val == 1 && req_rdy == 1;

  bool next_valid = occupied;
  AccNormReq next_entry = *resp_entry_Q_;
  if (push) {
    next_valid = true;
    next_entry = *req_bits;
    trace("normalizer: accepted acc_laddr=0x%x len=%u stats_id=%u norm_cmd=%u cmd_id=%u",
          static_cast<unsigned>(next_entry.acc_read_resp.laddr.raw),
          static_cast<unsigned>(next_entry.cmd.len),
          static_cast<unsigned>(next_entry.cmd.stats_id),
          static_cast<unsigned>(next_entry.cmd.cmd),
          static_cast<unsigned>(next_entry.acc_read_resp.cmd_id));
  } else if (pop) {
    next_valid = false;
    next_entry = AccNormReq{};
  }
  resp_valid_D_ = bit(next_valid);
  resp_entry_D_ = next_entry;
}

void Normalizer::reset() {
  resp_valid_Q_.reset(0);
  resp_valid_D_.reset(0);
  resp_entry_Q_.reset(AccNormReq{});
  resp_entry_D_.reset(AccNormReq{});
  req_rdy.reset(0);
  resp_val.reset(0);
  resp_bits.reset(AccNormReq{});
}

} // namespace smesh
