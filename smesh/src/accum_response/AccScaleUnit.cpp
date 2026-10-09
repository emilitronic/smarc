// **********************************************************************
// smesh/src/accum_response/AccScaleUnit.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Ordinary accumulator scaling and response buffering.
*/

#include "AccScaleUnit.hpp"
#include "AccScaleMath.hpp"

namespace smesh {

AccScaleUnit::AccScaleUnit(std::string /*name*/, IMPL_CTOR) {
  out_valid_Q_ <= out_valid_D_;
  out_entry_Q_ <= out_entry_D_;
  UPDATE(updateView).reads(out_valid_Q_, out_entry_Q_).writes(out_val, out_bits);
  UPDATE(updateReady).reads(out_valid_Q_, out_entry_Q_, out_rdy_issue,out_rdy_exresp).writes(req_rdy);
  UPDATE(updateBuffer).reads(out_valid_Q_, out_entry_Q_, out_rdy_issue, out_rdy_exresp, req_rdy, req_val, req_bits)
                      .writes(out_valid_D_, out_entry_D_);
}

void AccScaleUnit::updateView() {
  const bool occupied = *out_valid_Q_ == 1;
  out_val             = bit(occupied);
  out_bits            = occupied ? *out_entry_Q_ : AccScaleResp{};
}

void AccScaleUnit::updateReady() {
  const bool occupied       = *out_valid_Q_ == 1;
  const auto current        = *out_entry_Q_;
  const bool selected_ready = current.from_dma != 0 ? out_rdy_issue != 0 : out_rdy_exresp != 0;
  const bool pop            = occupied && selected_ready;
  req_rdy                   = bit(!occupied || pop);
}

void AccScaleUnit::updateBuffer() {
  const bool occupied       = *out_valid_Q_ == 1;
  const auto current        = *out_entry_Q_;
  const bool selected_ready = current.from_dma != 0 ? out_rdy_issue != 0 : out_rdy_exresp != 0;
  const bool pop            = occupied && selected_ready;
  const bool push           = req_val == 1 && req_rdy == 1;

  bool next_valid = occupied;
  AccScaleResp next_entry = current;
  if (push) {
    const auto acc         = req_bits->norm.acc_read_resp;
    assert_always(acc.act == 0, "AccScaleUnit activation math is not implemented");
    next_entry.full_data   = acc.data;
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      next_entry.data[lane] = scaleAccumValue(acc.data[lane], acc.scale);
    }
    next_entry.acc_bank_id = static_cast<u16>(acc.laddr.acc_bank());
    next_entry.from_dma    = acc.from_dma;
    next_valid             = true;
    trace("acc_scale_unit: accepted acc_laddr=0x%x bank=%u len=%u cmd_id=%u",
          static_cast<unsigned>(acc.laddr.raw),
          static_cast<unsigned>(next_entry.acc_bank_id),
          static_cast<unsigned>(acc.len),
          static_cast<unsigned>(acc.cmd_id));
  } else if (pop) {
    next_valid = false;
    next_entry = AccScaleResp{};
  }
  out_valid_D_ = bit(next_valid);
  out_entry_D_ = next_entry;
}

void AccScaleUnit::reset() {
  out_valid_Q_.reset(0);
  out_valid_D_.reset(0);
  out_entry_Q_.reset(AccScaleResp{});
  out_entry_D_.reset(AccScaleResp{});
  req_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(AccScaleResp{});
}

} // namespace smesh
