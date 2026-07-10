// **********************************************************************
// smesh/src/DmaReadCompletionMux.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
DMA read completion mux implementation.
*/

#include "DmaReadCompletionMux.hpp"

namespace smesh {

DmaReadCompletionMux::DmaReadCompletionMux(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(spad_in, accum_in).writes(dma_resp);
}

void DmaReadCompletionMux::update() {
  if (dma_resp.full()) {
    return;
  }

  if (!spad_in.empty()) {
    const auto completion = spad_in.pop();
    dma_resp.push(completion);
    trace("dma_read_completion_mux: spad cmd_id=%u bytes=%u",
          static_cast<unsigned>(completion.cmd_id),
          static_cast<unsigned>(completion.bytes_read));
    return;
  }

  if (!accum_in.empty()) {
    const auto completion = accum_in.pop();
    dma_resp.push(completion);
    trace("dma_read_completion_mux: accum cmd_id=%u bytes=%u",
          static_cast<unsigned>(completion.cmd_id),
          static_cast<unsigned>(completion.bytes_read));
  }
}

} // namespace smesh
