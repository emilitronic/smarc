// **********************************************************************
// smesh/src/ArbComplete.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 25 2026
/*
Completion arbiter for controller-to-RS completion tags.
*/

#include "ArbComplete.hpp"

namespace smesh {

ArbExLdStComplete::ArbExLdStComplete(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(ex_completed_val, ex_completed_bits, ld_completed,
             st_completed_val, st_completed_bits)
      .writes(rs_completed, st_completed_rdy);
}

void ArbExLdStComplete::update() {
  st_completed_rdy = bit(!rs_completed.full() && ex_completed_val == 0 && ld_completed.empty());
  if (rs_completed.full()) {
    return;
  }

  if (ex_completed_val != 0) {
    rs_completed.push(*ex_completed_bits);
    return;
  }

  if (!ld_completed.empty()) {
    rs_completed.push(ld_completed.pop());
    return;
  }

  if (st_completed_val == 1) {
    rs_completed.push(*st_completed_bits);
  }
}

void ArbExLdStComplete::reset() {
  st_completed_rdy.reset(0);
}

} // namespace smesh
