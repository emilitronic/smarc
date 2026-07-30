// **********************************************************************
// smesh/src/ExCtrlMeshCntlDeqCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026

#include "ExCtrlMeshCntlDeqCtrl.hpp"
#include "ExCtrlState.hpp"

namespace smesh {

ExCtrlMeshCntlDeqCtrl::ExCtrlMeshCntlDeqCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(control_state,
             cntl_val,
             cntl_bits,
             mesh_a_fire,
             mesh_b_fire,
             mesh_d_fire,
             mesh_a_rdy,
             mesh_b_rdy)
      .reads(mesh_d_rdy,
             mesh_req_rdy)
      .writes(mesh_cntl_deq_rdy, mesh_cntl_deq_fire, mesh_cntl_req_val);
}

void ExCtrlMeshCntlDeqCtrl::update() {
  const auto cntl = *cntl_bits;

  const auto next_mesh_cntl_deq_rdy = bit(
      (cntl.a_fire == 0 || mesh_a_fire != 0 || mesh_a_rdy == 0) &&
      (cntl.b_fire == 0 || mesh_b_fire != 0 || mesh_b_rdy == 0) &&
      (cntl.d_fire == 0 || mesh_d_fire != 0 || mesh_d_rdy == 0) &&
      (cntl.first == 0 || mesh_req_rdy != 0));

  mesh_cntl_deq_rdy = next_mesh_cntl_deq_rdy;
  mesh_cntl_deq_fire = bit(cntl_val != 0 && next_mesh_cntl_deq_rdy != 0);
  // valid signal to mesher's request port
  mesh_cntl_req_val = bit(control_state == static_cast<std::uint8_t>(ExCtrlFsmState::Flush));
  if (cntl_val != 0) {
    mesh_cntl_req_val = bit(mesh_cntl_deq_fire != 0 && (cntl.a_fire != 0 || cntl.b_fire != 0 || cntl.d_fire != 0));
  }
}

} // namespace smesh
