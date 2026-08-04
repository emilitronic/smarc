// **********************************************************************
// smesh/src/Mesher.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "Mesher.hpp"

namespace smesh {

Mesher::Mesher(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReqReady)
      .writes(req_rdy);
  UPDATE(updateReqState)
      .reads(req_val,
             req_bits);
  UPDATE(updateDataReady)
      .writes(a_rdy,
              b_rdy,
              d_rdy);
  UPDATE(updateOutputs)
      .writes(resp_val,
              resp_bits,
              tags_in_progress);
}

void Mesher::updateReqReady() {
  req_rdy = bit(!req_state_valid_);
}

void Mesher::updateReqState() {
  if (req_state_valid_ || req_val == 0) {
    return;
  }

  req_state_ = *req_bits;
  req_state_valid_ = true;

  trace("mesher: accepted req tag_valid=%u tag=%u rows=%u cols=%u flush=%u",
        static_cast<unsigned>(req_state_.tag.rs_tag_valid),
        static_cast<unsigned>(req_state_.tag.rs_tag),
        static_cast<unsigned>(req_state_.tag.rows),
        static_cast<unsigned>(req_state_.tag.cols),
        static_cast<unsigned>(req_state_.flush));
}

void Mesher::updateDataReady() {
  a_rdy = 1;
  b_rdy = 1;
  d_rdy = 1;
}

void Mesher::updateOutputs() {
  resp_val = 0;
  resp_bits = MesherResp{};

  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    tags_in_progress[i] = MesherTag{};
  }
}

void Mesher::reset() {
  req_state_       = ExCtrlMeshReq{};
  req_state_valid_ = false;

  req_rdy.reset(0);
  a_rdy.reset(0);
  b_rdy.reset(0);
  d_rdy.reset(0);
  resp_val.reset(0);
  resp_bits.reset(MesherResp{});

  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    tags_in_progress[i].reset(MesherTag{});
  }
}

} // namespace smesh
