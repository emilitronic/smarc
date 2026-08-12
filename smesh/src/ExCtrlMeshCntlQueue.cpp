// **********************************************************************
// smesh/src/ExCtrlMeshCntlQueue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "ExCtrlMeshCntlQueue.hpp"

namespace smesh {

namespace {
// helper to convert ExCtrlMeshCntl to ExCtrlMeshReq, the request into Mesher
ExCtrlMeshReq makeMeshReq(const ExCtrlMeshCntl& cntl) {
  ExCtrlMeshReq req{};
  req.pe_control.dataflow  = cntl.dataflow;
  req.pe_control.propagate = cntl.prop;
  req.pe_control.shift     = cntl.shift;
  req.a_transpose          = cntl.a_transpose;
  req.bd_transpose         = cntl.bd_transpose;
  req.total_rows           = cntl.total_rows;
  req.tag.rs_tag_valid     = cntl.rs_tag_valid;
  req.tag.rs_tag           = cntl.rs_tag;
  req.tag.addr             = cntl.c_addr;
  req.tag.rows             = cntl.c_rows;
  req.tag.cols             = cntl.c_cols;
  req.flush = 0;
  return req;
}

} // namespace

ExCtrlMeshCntlQueue::ExCtrlMeshCntlQueue(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateEnqReady).writes(enq_rdy);
  UPDATE(updateEnqAccept).reads(enq_val, enq_bits);
  UPDATE(updateDeqView).writes(cntl_val, cntl_bits, mesh_req_bits);
  UPDATE(updateDeqPop).reads(mesh_cntl_deq_rdy);
}
// can I accept an enqueue request?  yes if the queue is not full
void ExCtrlMeshCntlQueue::updateEnqReady() {
  enq_rdy = bit(count_ < kDepth);
}
// mutate queue state when enq handshake fires
void ExCtrlMeshCntlQueue::updateEnqAccept() {
  if (enq_val == 0 || count_ >= kDepth) {
    return;
  }

  entries_[tail_] = *enq_bits;
  const auto accepted = entries_[tail_];
  tail_ = (tail_ + 1) % kDepth;
  ++count_;

  trace("ex_mesh_cntl_q: accepted mulpre=%u mul=%u preload=%u a_fire=%u b_fire=%u d_fire=%u tag=%u",
        static_cast<unsigned>(accepted.perform_mul_pre),
        static_cast<unsigned>(accepted.perform_single_mul),
        static_cast<unsigned>(accepted.perform_single_preload),
        static_cast<unsigned>(accepted.a_fire),
        static_cast<unsigned>(accepted.b_fire),
        static_cast<unsigned>(accepted.d_fire),
        static_cast<unsigned>(accepted.rs_tag));
}
// what is at the head of the queue?
void ExCtrlMeshCntlQueue::updateDeqView() {
  const bool valid = count_ != 0; // non-zero count means you've got a valid entry at head
  const auto front = valid ? entries_[head_] : ExCtrlMeshCntl{};
  cntl_val      = bit(valid);
  cntl_bits     = front;
  mesh_req_bits = valid ? makeMeshReq(front) : ExCtrlMeshReq{};
}
// mutate queue state when deq handshake fires
void ExCtrlMeshCntlQueue::updateDeqPop() {
  if (count_ == 0 || mesh_cntl_deq_rdy == 0) {
    return;
  }

  trace("ex_mesh_cntl_q: issued tag=%u", static_cast<unsigned>(entries_[head_].rs_tag));

  entries_[head_] = ExCtrlMeshCntl{};
  head_ = (head_ + 1) % kDepth;
  --count_;
}

void ExCtrlMeshCntlQueue::reset() {
  entries_ = {};
  head_    = 0;
  tail_    = 0;
  count_   = 0;

  enq_rdy.reset(0);
  cntl_val.reset(0);
  cntl_bits.reset(ExCtrlMeshCntl{});
  mesh_req_bits.reset(ExCtrlMeshReq{});
}

} // namespace smesh
