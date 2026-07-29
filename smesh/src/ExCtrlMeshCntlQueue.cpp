// **********************************************************************
// smesh/src/ExCtrlMeshCntlQueue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "ExCtrlMeshCntlQueue.hpp"

namespace smesh {

namespace {

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
  UPDATE(updateDeqView).writes(deq_val, deq_bits, mesh_req_bits);
  UPDATE(updateDeqPop).reads(deq_rdy);
}

void ExCtrlMeshCntlQueue::updateEnqReady() {
  enq_rdy = bit(!valid_);
}

void ExCtrlMeshCntlQueue::updateEnqAccept() {
  if (enq_val == 0 || valid_) {
    return;
  }

  entry_ = *enq_bits;
  valid_ = true;

  trace("ex_mesh_cntl_q: accepted mulpre=%u mul=%u preload=%u a_fire=%u b_fire=%u d_fire=%u tag=%u",
        static_cast<unsigned>(entry_.perform_mul_pre),
        static_cast<unsigned>(entry_.perform_single_mul),
        static_cast<unsigned>(entry_.perform_single_preload),
        static_cast<unsigned>(entry_.a_fire),
        static_cast<unsigned>(entry_.b_fire),
        static_cast<unsigned>(entry_.d_fire),
        static_cast<unsigned>(entry_.rs_tag));
}

void ExCtrlMeshCntlQueue::updateDeqView() {
  deq_val = bit(valid_);
  deq_bits = valid_ ? entry_ : ExCtrlMeshCntl{};
  mesh_req_bits = valid_ ? makeMeshReq(entry_) : ExCtrlMeshReq{};
}

void ExCtrlMeshCntlQueue::updateDeqPop() {
  if (!valid_ || deq_rdy == 0) {
    return;
  }

  trace("ex_mesh_cntl_q: issued tag=%u", static_cast<unsigned>(entry_.rs_tag));

  valid_ = false;
  entry_ = ExCtrlMeshCntl{};
}

void ExCtrlMeshCntlQueue::reset() {
  valid_ = false;
  entry_ = ExCtrlMeshCntl{};

  enq_rdy.reset(0);
  deq_val.reset(0);
  deq_bits.reset(ExCtrlMeshCntl{});
  mesh_req_bits.reset(ExCtrlMeshReq{});
}

} // namespace smesh
