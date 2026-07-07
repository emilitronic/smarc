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
  UPDATE(updateRequest).reads(req_in).writes(mem_req);
  UPDATE(updateResponse).reads(mem_resp);
}

void DmaReader::updateRequest() {
  if (waiting_ || req_in.empty() || mem_req.full()) {
    return;
  }

  active_ = req_in.pop();
  const auto bytes = static_cast<std::uint16_t>(active_.cols);
  assert_always(bytes > 0 && bytes <= sizeof(std::uint64_t),
                "DmaReader currently supports one 1-to-8-byte row");

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
  if (!waiting_ || mem_resp.empty()) {
    return;
  }

  const auto resp = mem_resp.pop();
  assert_always(static_cast<std::uint16_t>(resp.id) ==
                    static_cast<std::uint16_t>(active_.cmd_id),
                "DmaReader response ID does not match active request");
  assert_always(static_cast<std::uint8_t>(resp.err) == 0,
                "DmaReader memory response reported an error");

  response_data_ = static_cast<std::uint64_t>(resp.rdata);
  response_valid_ = true;
  waiting_ = false;

  trace("dma_reader: response data=0x%llx cmd_id=%u",
        static_cast<unsigned long long>(response_data_),
        static_cast<unsigned>(resp.id));
}

void DmaReader::reset() {
  waiting_ = false;
  response_valid_ = false;
  active_ = {};
  response_data_ = 0;
}

} // namespace smesh
