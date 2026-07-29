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
      .reads(d_ready,
             cntl_a_fire,
             cntl_b_fire,
             cntl_d_fire,
             cntl_first,
             mesh_a_fire,
             mesh_b_fire,
             mesh_d_fire)
      .reads(mesh_a_rdy, mesh_b_rdy, mesh_d_rdy, mesh_req_rdy)
      .writes(firing, a_fire, b_fire, d_fire, mesh_cntl_deq_rdy);
}

void ExCtrlFeedSignals::update() {
  firing = bit(start_inputting_a != 0 || start_inputting_b != 0 || start_inputting_d != 0);
  a_fire = bit(a_valid != 0 && a_ready != 0);
  b_fire = bit(b_valid != 0 && b_ready != 0);
  d_fire = bit(d_valid != 0 && d_ready != 0);

  mesh_cntl_deq_rdy = bit(
      (cntl_a_fire == 0 || mesh_a_fire != 0 || mesh_a_rdy == 0) &&
      (cntl_b_fire == 0 || mesh_b_fire != 0 || mesh_b_rdy == 0) &&
      (cntl_d_fire == 0 || mesh_d_fire != 0 || mesh_d_rdy == 0) &&
      (cntl_first == 0 || mesh_req_rdy != 0));
}

} // namespace smesh
