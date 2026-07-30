// **********************************************************************
// smesh/include/ExCtrlMeshCntlDeqCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026
/*
Mesh-control dequeue-ready logic.

This block decides when the head entry of ExCtrlMeshCntlQueue may be released
toward the mesh boundary.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlMeshCntlQueue.hpp"

namespace smesh {

class ExCtrlMeshCntlDeqCtrl : public Component {
  DECLARE_COMPONENT(ExCtrlMeshCntlDeqCtrl);

 public:
  ExCtrlMeshCntlDeqCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(ExCtrlMeshCntl, cntl_bits); // head entry from ExCtrlMeshCntlQueue
  Input(bit, mesh_a_fire);          // Mesher A input accepted this cycle
  Input(bit, mesh_b_fire);          // Mesher B input accepted this cycle
  Input(bit, mesh_d_fire);          // Mesher D input accepted this cycle
  Input(bit, mesh_a_rdy);           // Mesher A input is ready
  Input(bit, mesh_b_rdy);           // Mesher B input is ready
  Input(bit, mesh_d_rdy);           // Mesher D input is ready
  Input(bit, mesh_req_rdy);         // Mesher request port is ready

  Output(bit, mesh_cntl_deq_rdy);   // mesh-control queue can release head entry

  void update();
};

} // namespace smesh
