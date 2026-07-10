// **********************************************************************
// smesh/src/DmaReader.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Minimal DMA reader implementation.
*/

#include "DmaReader.hpp"

namespace smesh {

DmaReader::DmaReader(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateRequest).reads(req_in).writes(mem_req);     // update reads from req_in & writes to mem_req
  UPDATE(updateResponse).reads(mem_resp).writes(resp_out);
}

void DmaReader::updateRequest() {
  if (waiting_ || req_in.empty() || mem_req.full()) {
    return;
  }

  active_ = req_in.pop();
  const auto bytes = static_cast<std::uint16_t>(active_.cols);
  assert_always(bytes > 0 && bytes <= sizeof(std::uint64_t), "DmaReader currently supports one 1-to-8-byte row");

  smem::MemReq req{};
  req.addr = active_.vaddr;
  req.size = u16(bytes);
  req.write = false;
  req.id = active_.cmd_id;
  mem_req.push(req);
  waiting_ = true;

  trace("dma_reader: read addr=0x%llx bytes=%u cmd_id=%u",
        static_cast<unsigned long long>(req.addr),
        static_cast<unsigned>(req.size),
        static_cast<unsigned>(req.id));
}

void DmaReader::updateResponse() {
  if (!waiting_ || mem_resp.empty() || resp_out.full()) {
    return;
  }

  const auto resp = mem_resp.pop();
  assert_always(static_cast<std::uint16_t>(resp.id) == static_cast<std::uint16_t>(active_.cmd_id), "DmaReader response ID does not match active request");
  assert_always(static_cast<std::uint8_t>(resp.err) == 0, "DmaReader memory response reported an error");

  const auto bytes = static_cast<std::uint16_t>(active_.cols);
  DmaReadResp dma_resp{};
  dma_resp.data          = resp.rdata;
  dma_resp.laddr         = active_.laddr;
  dma_resp.mask          = u8(bytes == 8 ? 0xffu : ((1u << bytes) - 1u));
  dma_resp.bytes_read    = u16(bytes);
  dma_resp.pixel_repeats = active_.pixel_repeats;
  dma_resp.cmd_id        = active_.cmd_id;
  dma_resp.last          = true;
  resp_out.push(dma_resp);
  waiting_               = false;

  trace("dma_reader: response data=0x%llx cmd_id=%u",
        static_cast<unsigned long long>(dma_resp.data),
        static_cast<unsigned>(resp.id));
}

void DmaReader::reset() {
  waiting_ = false;
  active_ = {};
}

} // namespace smesh
