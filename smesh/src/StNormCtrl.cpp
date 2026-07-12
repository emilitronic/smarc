// **********************************************************************
// smesh/src/StNormCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Store-path normalization-stage control implementation.
*/

#include "StNormCtrl.hpp"

namespace smesh {

StNormCtrl::StNormCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(norm_deq_rdy,
                        scale_enq_val,
                        normalizer_cmd_val,
                        accum_read_resp_rdy);
}

void StNormCtrl::update() {
  norm_deq_rdy = bit(false);
  scale_enq_val = bit(false);
  normalizer_cmd_val = bit(false);
  accum_read_resp_rdy = bit(false);
}

} // namespace smesh
