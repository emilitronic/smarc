// **********************************************************************
// smesh/src/ExCtrlFeedSignals.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "ExCtrlFeedSignals.hpp"

namespace smesh {

ExCtrlFeedSignals::ExCtrlFeedSignals(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(start_inputting_a,
             start_inputting_b,
             start_inputting_d,
             a_valid,
             b_valid,
             d_valid,
             a_ready,
             b_ready)
      .reads(d_ready)
      .writes(firing, a_fire, b_fire, d_fire);
}

void ExCtrlFeedSignals::update() {
  firing = bit(start_inputting_a != 0 || start_inputting_b != 0 || start_inputting_d != 0);
  a_fire = bit(a_valid != 0 && a_ready != 0);
  b_fire = bit(b_valid != 0 && b_ready != 0);
  d_fire = bit(d_valid != 0 && d_ready != 0);
}

} // namespace smesh
