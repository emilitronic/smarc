// **********************************************************************
// smesh/src/SpadReadPipes.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Scratchpad read response pipe implementations.
*/

#include "SpadReadPipes.hpp"

namespace smesh {
// deals with spad read resp to req from DMA path
SpadDmaReadPipe::SpadDmaReadPipe(std::string /*name*/, IMPL_CTOR) {
  out_valid_Q_ <= out_valid_D_;
  out_entry_Q_ <= out_entry_D_;

  UPDATE(updateOutView)
      .reads(out_valid_Q_, out_entry_Q_)
      .writes(out_val, out_bits);
  UPDATE(updateBuffer)
      .reads(resp_val, resp_bits, out_rdy, out_valid_Q_, out_entry_Q_)
      .writes(resp_rdy, out_valid_D_, out_entry_D_);
}

void SpadDmaReadPipe::updateOutView() {
  const bool occupied = *out_valid_Q_ == 1;
  out_val = bit(occupied);
  out_bits = occupied ? *out_entry_Q_ : SpadReadResp{};
}

void SpadDmaReadPipe::updateBuffer() {
  const bool occupied = *out_valid_Q_ == 1;
  const bool pop = occupied && out_rdy == 1;
  const auto resp = *resp_bits;
  const bool route_matches = resp.from_dma != 0;
  const bool slot_available = !occupied || pop;

  resp_rdy = bit(route_matches && slot_available);

  const bool push = resp_val == 1 && resp_rdy == 1;
  bool next_valid = occupied;
  SpadReadResp next_entry = *out_entry_Q_;

  if (push) {
    assert_always(route_matches, "SpadDmaReadPipe received non-DMA spad read response");
    next_valid = true;
    next_entry = resp;
    accepted_response_ = true;
    last_response_ = resp;

    trace("spad_dma_read_pipe: accepted laddr=0x%x len=%u cmd_id=%u",
          static_cast<unsigned>(resp.laddr.raw),
          static_cast<unsigned>(resp.len),
          static_cast<unsigned>(resp.cmd_id));
  } else if (pop) {
    next_valid = false;
    next_entry = SpadReadResp{};
  }

  out_valid_D_ = bit(next_valid);
  out_entry_D_ = next_entry;
}

void SpadDmaReadPipe::reset() {
  accepted_response_ = false;
  last_response_ = SpadReadResp{};
  out_valid_Q_.reset(0);
  out_entry_Q_.reset(SpadReadResp{});
  out_valid_D_.reset(0);
  out_entry_D_.reset(SpadReadResp{});
  resp_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SpadReadResp{});
}

SpadExReadPipe::SpadExReadPipe(std::string /*name*/, IMPL_CTOR) {
  out_valid_Q_ <= out_valid_D_;
  out_entry_Q_ <= out_entry_D_;

  UPDATE(updateOutView)
      .reads(out_valid_Q_, out_entry_Q_)
      .writes(out_val, out_bits);
  UPDATE(updateBuffer)
      .reads(resp_val, resp_bits, out_rdy, out_valid_Q_, out_entry_Q_)
      .writes(resp_rdy, out_valid_D_, out_entry_D_);
}

void SpadExReadPipe::updateOutView() {
  const bool occupied = *out_valid_Q_ == 1;
  out_val = bit(occupied);
  out_bits = occupied ? *out_entry_Q_ : SpadReadResp{};
}

void SpadExReadPipe::updateBuffer() {
  const bool occupied = *out_valid_Q_ == 1;
  const bool pop = occupied && out_rdy == 1;
  const auto resp = *resp_bits;
  const bool route_matches = resp.from_dma == 0;
  const bool slot_available = !occupied || pop;

  resp_rdy = bit(route_matches && slot_available);

  const bool push = resp_val == 1 && resp_rdy == 1;
  bool next_valid = occupied;
  SpadReadResp next_entry = *out_entry_Q_;

  if (push) {
    assert_always(route_matches, "SpadExReadPipe received DMA spad read response");
    next_valid = true;
    next_entry = resp;

    trace("spad_ex_read_pipe: accepted laddr=0x%x len=%u cmd_id=%u",
          static_cast<unsigned>(resp.laddr.raw),
          static_cast<unsigned>(resp.len),
          static_cast<unsigned>(resp.cmd_id));
  } else if (pop) {
    next_valid = false;
    next_entry = SpadReadResp{};
  }

  out_valid_D_ = bit(next_valid);
  out_entry_D_ = next_entry;
}

void SpadExReadPipe::reset() {
  out_valid_Q_.reset(0);
  out_entry_Q_.reset(SpadReadResp{});
  out_valid_D_.reset(0);
  out_entry_D_.reset(SpadReadResp{});
  resp_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SpadReadResp{});
}

} // namespace smesh
