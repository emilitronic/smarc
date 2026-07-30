// **********************************************************************
// smesh/src/ExCtrlMeshCntlDeqCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026

#include "ExCtrlMeshCntlDeqCtrl.hpp"

namespace smesh {

ExCtrlMeshCntlDeqCtrl::ExCtrlMeshCntlDeqCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(cntl_bits,
             mesh_a_fire,
             mesh_b_fire,
             mesh_d_fire,
             mesh_a_rdy,
             mesh_b_rdy,
             mesh_d_rdy,
             mesh_req_rdy)
      .writes(mesh_cntl_deq_rdy);
}

void ExCtrlMeshCntlDeqCtrl::update() {
  const auto cntl = *cntl_bits;

  mesh_cntl_deq_rdy = bit(
      (cntl.a_fire == 0 || mesh_a_fire != 0 || mesh_a_rdy == 0) &&
      (cntl.b_fire == 0 || mesh_b_fire != 0 || mesh_b_rdy == 0) &&
      (cntl.d_fire == 0 || mesh_d_fire != 0 || mesh_d_rdy == 0) &&
      (cntl.first == 0 || mesh_req_rdy != 0));
}

} // namespace smesh
