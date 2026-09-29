// **********************************************************************
// smesh/src/dma/DmaMemMux.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 28 2026

#include "DmaMemMux.hpp"

namespace smesh {

namespace {
// Reserve the high ID bit for write acknowledgements at the shared memory port.
constexpr std::uint16_t kWriteIdBit = 0x8000u;
}

DmaMemMux::DmaMemMux(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateRequest).reads(read_req, write_req).writes(mem_req);
  UPDATE(updateResponse).reads(mem_resp).writes(read_resp);
}

void DmaMemMux::updateRequest() {
  if (mem_req.full()) return;

  smem::MemReq req{};
  // One memory request per cycle; reads take priority while both are queued.
  if (!read_req.empty()) {         // load-side reader req
    req = read_req.pop();
    assert_always(!req.write && !(static_cast<std::uint16_t>(req.id) & kWriteIdBit),
                  "DMA read request ID overlaps write ID namespace");
  } else if (!write_req.empty()) { // store-side writer req
    req = write_req.pop();
    assert_always(req.write && !(static_cast<std::uint16_t>(req.id) & kWriteIdBit),
                  "DMA write request ID overlaps write ID namespace");
    // set MSB of req.id for write (when resp comes back this let's us know its just a write ack, not read data)
    req.id = static_cast<std::uint16_t>(req.id) | kWriteIdBit;
  } else {
    return;
  }
  mem_req.push(req);
}

void DmaMemMux::updateResponse() {
  if (mem_resp.empty()) return;
  const auto& head = mem_resp.peek(); // look at oldest response in FIFO queue
  // if its not a write ack but reader is full, we can't pop it yet (we'll try again next cycle)
  if (!(static_cast<std::uint16_t>(head.id) & kWriteIdBit) && read_resp.full()) return;
  // if it is a write ack or reader is not full we can pop it
  const auto resp = mem_resp.pop();
  // if it is not a write ack, send resp to reader (else its dropped)
  if (!(static_cast<std::uint16_t>(resp.id) & kWriteIdBit)) {
    read_resp.push(resp);
  }
}

} // namespace smesh
