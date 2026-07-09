// **********************************************************************
// smesh/src/SmeshTop.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Top-level smesh composition point.
*/

#include "SmeshTop.hpp"

namespace smesh {

SmeshTop::SmeshTop(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update);
}

void SmeshTop::update() {}

void SmeshTop::reset() {
  trace("smesh_top: reset");
}

} // namespace smesh
