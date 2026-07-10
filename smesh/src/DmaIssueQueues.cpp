// **********************************************************************
// smesh/src/DmaIssueQueues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 10 2026
/*
DMA issue queue implementations.
*/

#include "DmaIssueQueues.hpp"

namespace smesh {

DmaReadIssueQueue::DmaReadIssueQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(req_in).writes(req_out);
}

void DmaReadIssueQueue::update() {
  if (req_in.empty() || req_out.full()) {
    return;
  }

  const auto req = req_in.pop();
  req_out.push(req);

  trace("dma_read_issue_queue: accepted vaddr=0x%llx laddr=0x%x cols=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.cols),
        static_cast<unsigned>(req.cmd_id));
}

DmaWriteIssueQueue::DmaWriteIssueQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(req_in).writes(req_out);
}

void DmaWriteIssueQueue::update() {
  if (req_in.empty() || req_out.full()) {
    return;
  }

  const auto req = req_in.pop();
  req_out.push(req);

  trace("dma_write_issue_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

} // namespace smesh
