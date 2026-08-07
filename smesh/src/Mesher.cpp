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



} // namespace smesh
