// **********************************************************************
// smesh/src/DmaWriter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-side DMA writer skeleton implementation.
*/

#include "DmaWriter.hpp"

namespace smesh {

DmaWriter::DmaWriter(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(req_rdy);
  UPDATE(update).reads(req_val, req_bits).writes(mem_req);
}

void DmaWriter::updateReady() {
  req_rdy = bit(!mem_req.full());
}

void DmaWriter::update() {
  if (req_val == 0 || mem_req.full()) {
    return;
  }

  const auto writer_req = *req_bits;
  const auto issue = writer_req.issue;
  smem::MemReq req{};
  req.addr = issue.vaddr;
  req.write = true;
  req.size = 8;
  req.id = issue.cmd_id;
  req.wdata = writer_req.data_is_all_zeros ? u64(0) : writer_req.data;
  mem_req.push(req);
  trace("dma_writer: store vaddr=0x%llx data=0x%llx cmd_id=%u",
        static_cast<unsigned long long>(req.addr),
        static_cast<unsigned long long>(req.wdata),
        static_cast<unsigned>(req.id));
}

} // namespace smesh
