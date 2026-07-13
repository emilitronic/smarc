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
  UPDATE(update).reads(resp_in).writes(resp_out);
}

void SpadDmaReadPipe::update() {
  if (resp_in.empty() || resp_out.full()) {
    return;
  }

  const auto resp = resp_in.pop();
  assert_always(resp.from_dma != 0, "SpadDmaReadPipe received non-DMA spad read response");
  resp_out.push(resp);
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
}

ExDmaReadPipe::ExDmaReadPipe(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(resp_in).writes(resp_out);
}

void ExDmaReadPipe::update() {
  if (resp_in.empty() || resp_out.full()) {
    return;
  }

  const auto resp = resp_in.pop();
  assert_always(resp.from_dma == 0, "ExDmaReadPipe received DMA spad read response");
  resp_out.push(resp);

  trace("ex_dma_read_pipe: accepted laddr=0x%x len=%u cmd_id=%u",
        static_cast<unsigned>(resp.laddr.raw),
        static_cast<unsigned>(resp.len),
        static_cast<unsigned>(resp.cmd_id));
}

} // namespace smesh
