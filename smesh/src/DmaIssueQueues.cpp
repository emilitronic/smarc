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
  entry_Q_ <= entry_D_;
  UPDATE(updateDeqView).reads(entry_Q_).writes(deq_val, deq_bits);
  UPDATE(updateEnqReady).reads(entry_Q_, deq_rdy).writes(req_rdy);
  UPDATE(updateStorage)
      .reads(entry_Q_, req_val, req_rdy, req_bits, deq_rdy)
      .writes(entry_D_);
}
// expose head of queue to outside logic
void DmaWriteDispatchQueue::updateDeqView() {
  const auto entry = *entry_Q_;
  deq_val = entry.valid;
  deq_bits = entry.valid == 1 ? entry.bits : DmaWriteReq{};
}
void DmaWriteDispatchQueue::updateEnqReady() {
  const auto entry = *entry_Q_;
  req_rdy = bit(entry.valid == 0 || deq_rdy == 1);
}
// A dequeue frees the slot for another request in the same cycle.
void DmaWriteDispatchQueue::updateStorage() {
  const auto current = *entry_Q_;
  auto next = current;
  const bool pop = current.valid == 1 && deq_rdy == 1;
  const bool push = req_val == 1 && req_rdy == 1;

  if (pop) {
    const auto& req = current.bits;
    trace("dma_write_dispatch_queue: accepted vaddr=0x%llx laddr=0x%x len=%u block=%u cmd_id=%u",
          static_cast<unsigned long long>(req.vaddr),
          static_cast<unsigned>(req.laddr.raw),
          static_cast<unsigned>(req.len),
          static_cast<unsigned>(req.block),
          static_cast<unsigned>(req.cmd_id));
    next = Entry{};
  }
  if (push) {
    next.valid = 1;
    next.bits = *req_bits;
  }
  entry_D_ = next;
}

void DmaWriteDispatchQueue::reset() {
  entry_D_.reset(Entry{});
  req_rdy.reset(0);
  deq_val.reset(0);
  deq_bits.reset(DmaWriteReq{});
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
