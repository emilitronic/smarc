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
  UPDATE(updateDeqView).reads(req_in).writes(deq_val, deq_bits);
  UPDATE(updateDeqPop).reads(deq_rdy);
}
// expose head of queue to outside logic
void DmaWriteDispatchQueue::updateDeqView() {
  deq_val = bit(!req_in.empty());  // if there's a command at head of queue, assert deq_val
  deq_bits = req_in.empty() ? DmaWriteReq{} : req_in.peek(); // if queue is empty, drive blank request, else expose head of queue w/o consuming it
}
// pop head of queue if outside logic says it's ok to advance
void DmaWriteDispatchQueue::updateDeqPop() {
  if (req_in.empty() || deq_rdy == 0) {  // don't consume command unless outside logic says this entry fires
    return;
  }

  const auto req = req_in.pop();

  trace("dma_write_dispatch_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

DmaWriteNormQueue::DmaWriteNormQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateEnqReady).writes(enq_rdy);
  UPDATE(updateDeqView).writes(deq_val, deq_bits);
  UPDATE(updateEnqAccept).reads(enq_val, enq_bits);
  UPDATE(updateDeqPop).reads(deq_rdy);
}

void DmaWriteNormQueue::updateEnqReady() {
  enq_rdy = bit(!valid_);
}

void DmaWriteNormQueue::updateEnqAccept() {
  if (enq_val == 0 || valid_) {
    return;
  }

  entry_ = *enq_bits;
  valid_ = true;

  trace("dma_write_norm_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(entry_.vaddr),
        static_cast<unsigned>(entry_.laddr.raw),
        static_cast<unsigned>(entry_.len),
        static_cast<unsigned>(entry_.block),
        static_cast<unsigned>(entry_.cmd_id));
}

void DmaWriteNormQueue::updateDeqView() {
  deq_val = bit(valid_);
  deq_bits = valid_ ? entry_ : DmaWriteReq{};
}

void DmaWriteNormQueue::updateDeqPop() {
  if (!valid_ || deq_rdy == 0) {
    return;
  }

  trace("dma_write_norm_queue: issued vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(entry_.vaddr),
        static_cast<unsigned>(entry_.laddr.raw),
        static_cast<unsigned>(entry_.len),
        static_cast<unsigned>(entry_.block),
        static_cast<unsigned>(entry_.cmd_id));

  valid_ = false;
  entry_ = DmaWriteReq{};
}

void DmaWriteNormQueue::reset() {
  valid_ = false;
  entry_ = DmaWriteReq{};
}

DmaWriteScaleQueue::DmaWriteScaleQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateEnqReady).writes(enq_rdy);
  UPDATE(updateEnqAccept).reads(enq_val, enq_bits);
  UPDATE(updateDeqView).writes(deq_val, deq_bits);
  UPDATE(updateDeqPop).reads(deq_rdy);
}

void DmaWriteScaleQueue::updateEnqReady() {
  enq_rdy = bit(!valid_);
}

void DmaWriteScaleQueue::updateEnqAccept() {
  if (enq_val == 0 || valid_) {
    return;
  }

  const auto req = *enq_bits;
  entry_ = req;
  valid_ = true;

  trace("dma_write_scale_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

void DmaWriteScaleQueue::updateDeqView() {
  deq_val = bit(valid_);
  deq_bits = valid_ ? entry_ : DmaWriteReq{};
}

void DmaWriteScaleQueue::updateDeqPop() {
  if (!valid_ || deq_rdy == 0) {
    return;
  }

  trace("dma_write_scale_queue: issued vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(entry_.vaddr),
        static_cast<unsigned>(entry_.laddr.raw),
        static_cast<unsigned>(entry_.len),
        static_cast<unsigned>(entry_.block),
        static_cast<unsigned>(entry_.cmd_id));

  valid_ = false;
  entry_ = DmaWriteReq{};
}

void DmaWriteScaleQueue::reset() {
  valid_ = false;
  entry_ = DmaWriteReq{};
}

DmaWriteIssueQueue::DmaWriteIssueQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateEnqReady).writes(enq_rdy);
  UPDATE(updateEnqAccept).reads(enq_val, enq_bits);
  UPDATE(updateDeqView).writes(deq_val, deq_bits);
  UPDATE(updateDeqPop).reads(deq_rdy);
}

void DmaWriteIssueQueue::updateEnqReady() {
  enq_rdy = bit(!valid_);
}

void DmaWriteIssueQueue::updateEnqAccept() {
  if (enq_val == 0 || valid_) {
    return;
  }

  const auto req = *enq_bits;
  entry_ = req;
  valid_ = true;

  trace("dma_write_issue_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.block),
        static_cast<unsigned>(req.cmd_id));
}

void DmaWriteIssueQueue::updateDeqView() {
  deq_val = bit(valid_);
  deq_bits = valid_ ? entry_ : DmaWriteReq{};
}

void DmaWriteIssueQueue::updateDeqPop() {
  if (!valid_ || deq_rdy == 0) {
    return;
  }

  trace("dma_write_issue_queue: issued vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
        static_cast<unsigned long long>(entry_.vaddr),
        static_cast<unsigned>(entry_.laddr.raw),
        static_cast<unsigned>(entry_.len),
        static_cast<unsigned>(entry_.block),
        static_cast<unsigned>(entry_.cmd_id));

  valid_ = false;
  entry_ = DmaWriteReq{};
}

void DmaWriteIssueQueue::reset() {
  valid_ = false;
  entry_ = DmaWriteReq{};
}

} // namespace smesh
