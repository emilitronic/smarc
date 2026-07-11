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

DmaWriteDispatchQueue::DmaWriteDispatchQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(req_in, deq_ready).writes(req_out, deq_valid, deq_bits);
}

void DmaWriteDispatchQueue::update() {
  deq_valid = bit(!req_in.empty());  // if there's a command at head of queue, assert deq_valid
  deq_bits = req_in.empty() ? DmaWriteReq{} : req_in.peek(); // if queue is empty, drive blank request, else expose head of queue w/o consuming it

  if (req_in.empty() || req_out.full() || deq_ready == 0) {  // don't move command forward if no command available, or next queue is full, or outside logic says not ready
    return;
  }

  const auto req = req_in.pop();
  req_out.push(req);

  trace("dma_write_dispatch_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

DmaWriteNormQueue::DmaWriteNormQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(enq_ready);
  UPDATE(update).reads(req_in).writes(req_out);
}

void DmaWriteNormQueue::updateReady() {
  enq_ready = bit(!req_out.full());
}

void DmaWriteNormQueue::update() {
  if (req_in.empty() || req_out.full()) {
    return;
  }

  const auto req = req_in.pop();
  req_out.push(req);

  trace("dma_write_norm_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

DmaWriteScaleQueue::DmaWriteScaleQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(req_in).writes(req_out);
}

void DmaWriteScaleQueue::update() {
  if (req_in.empty() || req_out.full()) {
    return;
  }

  const auto req = req_in.pop();
  req_out.push(req);

  trace("dma_write_scale_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
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
