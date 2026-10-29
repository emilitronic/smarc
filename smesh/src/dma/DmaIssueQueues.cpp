// **********************************************************************
// smesh/src/dma/DmaIssueQueues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 10 2026
/*
DMA issue queue implementations.
*/

#include "DmaIssueQueues.hpp"

namespace smesh {

DmaReadIssueQueue::DmaReadIssueQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).reads(req_out).writes(req_rdy);
  UPDATE(update).reads(req_val, req_rdy, req_bits).writes(req_out);
}

void DmaReadIssueQueue::updateReady() {
  req_rdy = bit(!req_out.full());
}

void DmaReadIssueQueue::update() {
  if (req_val == 0 || req_rdy == 0) {
    return;
  }

  const auto req = *req_bits;
  req_out.push(req);

  trace("dma_read_issue_queue: accepted vaddr=0x%llx laddr=0x%x cols=%u cmd_id=%u",
        static_cast<unsigned long long>(req.vaddr),
        static_cast<unsigned>(req.laddr.raw),
        static_cast<unsigned>(req.cols),
        static_cast<unsigned>(req.cmd_id));
}

void DmaReadIssueQueue::reset() {
  req_rdy.reset(0);
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
  entry_Q_ <= entry_D_;
  UPDATE(updateEnqReady).reads(entry_Q_).writes(enq_rdy);
  UPDATE(updateDeqView).reads(entry_Q_).writes(deq_val, deq_bits);
  UPDATE(updateStorage).reads(entry_Q_, enq_val, enq_rdy, enq_bits, deq_rdy)
                       .writes(entry_D_);
}

void DmaWriteNormQueue::updateEnqReady() {
  // Keep the original one-entry queue's empty-slot-only acceptance policy.
  enq_rdy = bit(entry_Q_->valid == 0);
}

void DmaWriteNormQueue::updateDeqView() {
  deq_val = entry_Q_->valid;
  deq_bits = entry_Q_->valid == 1 ? entry_Q_->bits : DmaWriteReq{};
}

void DmaWriteNormQueue::updateStorage() {
  auto next = *entry_Q_;
  const bool pop = next.valid == 1 && deq_rdy == 1;
  const bool push = enq_val == 1 && enq_rdy == 1;
  if (!pop && !push) return;
  if (pop) next = Entry{};
  if (push) {
    next.valid = 1;
    next.bits = *enq_bits;
  }
  entry_D_ = next;
  trace("dma_write_norm_queue: pop=%u push=%u valid=%u cmd_id=%u\n",
        unsigned(pop), unsigned(push), unsigned(next.valid), unsigned(next.bits.cmd_id));
}

void DmaWriteNormQueue::reset() {
  entry_D_.reset(Entry{});
  enq_rdy.reset(0);
  deq_val.reset(0);
  deq_bits.reset(DmaWriteReq{});
}

DmaWriteScaleQueue::DmaWriteScaleQueue(std::string /*name*/, IMPL_CTOR) {
  entry_Q_ <= entry_D_;
  UPDATE(updateEnqReady).reads(entry_Q_).writes(enq_rdy);
  UPDATE(updateDeqView).reads(entry_Q_).writes(deq_val, deq_bits);
  UPDATE(updateStorage).reads(entry_Q_, enq_val, enq_rdy, enq_bits, deq_rdy)
                       .writes(entry_D_);
}

void DmaWriteScaleQueue::updateEnqReady() {
  // Keep the original one-entry queue's empty-slot-only acceptance policy.
  enq_rdy = bit(entry_Q_->valid == 0);
}

void DmaWriteScaleQueue::updateDeqView() {
  deq_val = entry_Q_->valid;
  deq_bits = entry_Q_->valid == 1 ? entry_Q_->bits : DmaWriteReq{};
}

void DmaWriteScaleQueue::updateStorage() {
  auto next = *entry_Q_;
  const bool pop = next.valid == 1 && deq_rdy == 1;
  const bool push = enq_val == 1 && enq_rdy == 1;
  if (!pop && !push) return;
  if (pop) next = Entry{};
  if (push) {
    next.valid = 1;
    next.bits = *enq_bits;
  }
  entry_D_ = next;
  trace("dma_write_scale_queue: pop=%u push=%u valid=%u cmd_id=%u\n",
        unsigned(pop), unsigned(push), unsigned(next.valid), unsigned(next.bits.cmd_id));
}

void DmaWriteScaleQueue::reset() {
  entry_D_.reset(Entry{});
  enq_rdy.reset(0);
  deq_val.reset(0);
  deq_bits.reset(DmaWriteReq{});
}

DmaWriteIssueQueue::DmaWriteIssueQueue(std::string /*name*/, IMPL_CTOR) {
  entry_Q_ <= entry_D_;
  UPDATE(updateEnqReady).reads(entry_Q_).writes(enq_rdy);
  UPDATE(updateDeqView).reads(entry_Q_).writes(deq_val, deq_bits);
  UPDATE(updateStorage).reads(entry_Q_, enq_val, enq_rdy, enq_bits, deq_rdy)
                       .writes(entry_D_);
}

void DmaWriteIssueQueue::updateEnqReady() {
  // Keep the original one-entry queue's empty-slot-only acceptance policy.
  enq_rdy = bit(entry_Q_->valid == 0);
}

void DmaWriteIssueQueue::updateDeqView() {
  deq_val = entry_Q_->valid;
  deq_bits = entry_Q_->valid == 1 ? entry_Q_->bits : DmaWriteReq{};
}

void DmaWriteIssueQueue::updateStorage() {
  auto next = *entry_Q_;
  const bool pop = next.valid == 1 && deq_rdy == 1;
  const bool push = enq_val == 1 && enq_rdy == 1;
  if (!pop && !push) return;
  if (pop) next = Entry{};
  if (push) {
    next.valid = 1;
    next.bits = *enq_bits;
  }
  entry_D_ = next;
  trace("dma_write_issue_queue: pop=%u push=%u valid=%u cmd_id=%u\n",
        unsigned(pop), unsigned(push), unsigned(next.valid), unsigned(next.bits.cmd_id));
}

void DmaWriteIssueQueue::reset() {
  entry_D_.reset(Entry{});
  enq_rdy.reset(0);
  deq_val.reset(0);
  deq_bits.reset(DmaWriteReq{});
}

} // namespace smesh
