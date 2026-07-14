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
  UPDATE(update).reads(issue_in, spad_data_in, acc_data_in).writes(mem_req);
}

void DmaWriter::update() {
  if (issue_in.empty() || mem_req.full()) {
    return;
  }

  const auto issue = issue_in.peek();
  const auto laddr = issue.laddr;

  smem::MemReq req{};
  req.addr = issue.vaddr;
  req.write = true;
  req.size = 8;
  req.id = issue.cmd_id;

  if (laddr.is_garbage()) {
    issue_in.pop();
    trace("dma_writer: dropped garbage store cmd_id=%u", static_cast<unsigned>(issue.cmd_id));
    return;
  }

  if (laddr.is_acc_addr()) {
    if (acc_data_in.empty()) {
      return;
    }
    const auto data = acc_data_in.pop();
    issue_in.pop();
    req.wdata = data.data;
    mem_req.push(req);
    trace("dma_writer: acc store vaddr=0x%llx data=0x%llx cmd_id=%u",
          static_cast<unsigned long long>(req.addr),
          static_cast<unsigned long long>(req.wdata),
          static_cast<unsigned>(req.id));
    return;
  }

  if (spad_data_in.empty()) {
    return;
  }
  const auto data = spad_data_in.pop();
  issue_in.pop();
  req.wdata = data.data;
  mem_req.push(req);
  trace("dma_writer: spad store vaddr=0x%llx data=0x%llx cmd_id=%u",
        static_cast<unsigned long long>(req.addr),
        static_cast<unsigned long long>(req.wdata),
        static_cast<unsigned>(req.id));
}

} // namespace smesh
