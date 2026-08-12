// **********************************************************************
// smesh/include/ExCtrlMeshCntlDeqCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026
/*
Mesh-control dequeue-ready logic.

This block decides when the head entry of ExCtrlMeshCntlQueue may be released
toward the mesh boundary.  That means both popped as well as have any Mesher
request packed validated.
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

  Input(u8, control_state);         // current ExCtrl FSM state
  Input(bit, cntl_val);             // ExCtrlMeshCntlQueue has a valid head entry
  Input(ExCtrlMeshCntl, cntl_bits); // head entry from ExCtrlMeshCntlQueue
  Input(bit, mesh_a_fire);          // Mesher A input accepted this cycle
  Input(bit, mesh_b_fire);          // Mesher B input accepted this cycle
  Input(bit, mesh_d_fire);          // Mesher D input accepted this cycle
  Input(bit, mesh_a_rdy);           // Mesher A input is ready
  Input(bit, mesh_b_rdy);           // Mesher B input is ready
  Input(bit, mesh_d_rdy);           // Mesher D input is ready
  Input(bit, mesh_req_rdy);         // Mesher request port is ready

  Output(bit, mesh_cntl_deq_rdy);   // mesh-control queue can pop head entry
  Output(bit, mesh_cntl_deq_fire);  // valid head entry is popped this cycle
  Output(bit, mesh_cntl_req_val);   // valid signal to Mesher's request port

  void update();
};

} // namespace smesh
