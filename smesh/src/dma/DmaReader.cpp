// **********************************************************************
// smesh/src/dma/DmaReader.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Minimal DMA reader implementation.
*/

#include "DmaReader.hpp"

#include <algorithm>

namespace smesh {

DmaReader::DmaReader(std::string /*name*/, IMPL_CTOR) {
  // One memory beat can produce two local tile rows.
  resp_out.setSize(2); // give FIFO room for 2 entries
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

  trace("dma_reader: read addr=0x%llx bytes=%u cmd_id=%u\n",
        static_cast<unsigned long long>(req.addr),
        static_cast<unsigned>(req.size),
        static_cast<unsigned>(req.id));
}

void DmaReader::updateResponse() {
  if (!waiting_ || mem_resp.empty()) {
    return;
  }

  const auto bytes = static_cast<std::uint16_t>(active_.cols); // number of cols in req 
  // segment = one local-mem row from DRAM resp; 
  const auto segments = active_.has_acc_bitwidth != 0 ? 1u : (bytes + kDim - 1) / kDim;
  if (resp_out.freeCount() < static_cast<int>(segments)) return;

  const auto resp = mem_resp.pop();
  assert_always(static_cast<std::uint16_t>(resp.id) == static_cast<std::uint16_t>(active_.cmd_id), "DmaReader response ID does not match active request");
  assert_always(static_cast<std::uint8_t>(resp.err) == 0, "DmaReader memory response reported an error");

  assert_always(segments == 1 || active_.block_stride > 0, "DmaReader needs a local block stride for multi-tile rows");
  // Complete each tile row after its local-memory write has been accepted.
  for (unsigned segment = 0; segment < segments; ++segment) {
    const auto byte_offset = segment * kDim;
    const auto chunk_bytes = static_cast<std::uint16_t>(segments == 1 ? bytes : std::min<std::size_t>(kDim, bytes - byte_offset));
    DmaReadResp dma_resp{};
    dma_resp.data = packDmaReadData(static_cast<std::uint64_t>(resp.rdata) >> (8 * byte_offset));
    dma_resp.laddr = active_.laddr + segment * static_cast<std::uint16_t>(active_.block_stride);
    dma_resp.mask = u8(chunk_bytes == 8 ? 0xffu : ((1u << chunk_bytes) - 1u));
    dma_resp.has_acc_bitwidth = active_.has_acc_bitwidth;
    dma_resp.scale = active_.scale;
    dma_resp.repeats = active_.repeats;
    dma_resp.len = chunk_bytes;
    dma_resp.bytes_read = u16(chunk_bytes);
    dma_resp.pixel_repeats = active_.pixel_repeats;
    dma_resp.cmd_id = active_.cmd_id;
    dma_resp.last = true;
    resp_out.push(dma_resp);
  }
  waiting_               = false;

  trace("dma_reader: response data=0x%llx cmd_id=%u\n",
        static_cast<unsigned long long>(resp.rdata),
        static_cast<unsigned>(resp.id));
}

void DmaReader::reset() {
  waiting_ = false;
  active_ = {};
}

} // namespace smesh
