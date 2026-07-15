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
  UPDATE(updateRespReady).reads(resp_val, resp_bits).writes(resp_rdy);
  UPDATE(updateOutView).writes(out_val, out_bits);
  UPDATE(updateOutPop).reads(out_rdy);
  UPDATE(updateAccept).reads(resp_val, resp_bits);
}

void SpadDmaReadPipe::updateRespReady() {
  const auto resp = *resp_bits;
  resp_rdy = bit(resp_val != 0 && resp.from_dma != 0 && !out_valid_);
}

void SpadDmaReadPipe::updateOutView() {
  out_val = bit(out_valid_);
  out_bits = out_valid_ ? out_entry_ : SpadReadResp{};
}

void SpadDmaReadPipe::updateOutPop() {
  if (out_valid_ && out_rdy != 0) {
    out_valid_ = false;
    out_entry_ = SpadReadResp{};
  }
}

void SpadDmaReadPipe::updateAccept() {
  const auto resp = *resp_bits;
  if (resp_val == 0 || resp.from_dma == 0 || out_valid_) {
    return;
  }

  assert_always(resp.from_dma != 0, "SpadDmaReadPipe received non-DMA spad read response");
  out_entry_ = resp;
  out_valid_ = true;
  accepted_response_ = true;
  last_response_     = resp;

  trace("spad_dma_read_pipe: accepted laddr=0x%x len=%u cmd_id=%u",
        static_cast<unsigned>(resp.laddr.raw),
        static_cast<unsigned>(resp.len),
        static_cast<unsigned>(resp.cmd_id));
}

void SpadDmaReadPipe::reset() {
  accepted_response_ = false;
  last_response_ = SpadReadResp{};
  out_valid_ = false;
  out_entry_ = SpadReadResp{};
}

SpadExReadPipe::SpadExReadPipe(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateRespReady).reads(resp_val, resp_bits).writes(resp_rdy);
  UPDATE(updateOutView).writes(out_val, out_bits);
  UPDATE(updateOutPop).reads(out_rdy);
  UPDATE(updateAccept).reads(resp_val, resp_bits);
}

void SpadExReadPipe::updateRespReady() {
  const auto resp = *resp_bits;
  resp_rdy = bit(resp_val != 0 && resp.from_dma == 0 && !out_valid_);
}

void SpadExReadPipe::updateOutView() {
  out_val = bit(out_valid_);
  out_bits = out_valid_ ? out_entry_ : SpadReadResp{};
}

void SpadExReadPipe::updateOutPop() {
  if (out_valid_ && out_rdy != 0) {
    out_valid_ = false;
    out_entry_ = SpadReadResp{};
  }
}

void SpadExReadPipe::updateAccept() {
  const auto resp = *resp_bits;
  if (resp_val == 0 || resp.from_dma != 0 || out_valid_) {
    return;
  }

  assert_always(resp.from_dma == 0, "SpadExReadPipe received DMA spad read response");
  out_entry_ = resp;
  out_valid_ = true;

  trace("spad_ex_read_pipe: accepted laddr=0x%x len=%u cmd_id=%u",
        static_cast<unsigned>(resp.laddr.raw),
        static_cast<unsigned>(resp.len),
        static_cast<unsigned>(resp.cmd_id));
}

void SpadExReadPipe::reset() {
  out_valid_ = false;
  out_entry_ = SpadReadResp{};
}

} // namespace smesh
