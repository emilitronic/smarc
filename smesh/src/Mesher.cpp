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
  const bool req_ready = true;
  const bool req_fire  = req_val != 0 && req_ready;

  (void) req_fire;

  req_rdy = bit(req_ready);
}

void Mesher::reset() {}

} // namespace smesh
