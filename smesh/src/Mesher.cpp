// **********************************************************************
// smesh/src/Mesher.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "Mesher.hpp"

namespace smesh {

Mesher::Mesher(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(req_val)
      .writes(req_rdy);
}

void Mesher::update() {
  const bool last_fire = false; // TODO: compute from row/input-advance completion logic.

  req_rdy = bit(!req_state_valid_ || last_fire);

  const bool req_fire = req_val != 0 && req_rdy != 0;
  (void) req_fire;
}

void Mesher::reset() {}

} // namespace smesh
