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
      .reads(ex_completed_val, ex_completed_bits, ld_completed, st_completed)
      .writes(rs_completed);
}

void ArbExLdStComplete::update() {
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

  if (!st_completed.empty()) {
    rs_completed.push(st_completed.pop());
  }
}

} // namespace smesh
