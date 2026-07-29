// **********************************************************************
// smesh/include/ExCtrlFeedSignals.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Small derived control signals for ExecuteController row feeding.
*/

#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class ExCtrlFeedSignals : public Component {
  DECLARE_COMPONENT(ExCtrlFeedSignals);

 public:
  ExCtrlFeedSignals(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, start_inputting_a);
  Input(bit, start_inputting_b);
  Input(bit, start_inputting_d);
  Input(bit, a_valid);
  Input(bit, b_valid);
  Input(bit, d_valid);
  Input(bit, a_ready);
  Input(bit, b_ready);
  Input(bit, d_ready);
  Input(bit, cntl_a_fire); // cntl entry expects A to feed mesh
  Input(bit, cntl_b_fire); // cntl entry expects B to feed mesh
  Input(bit, cntl_d_fire); // cntl entry expects D to feed mesh
  Input(bit, cntl_first);  // cntl entry must also send mesh request
  Input(bit, mesh_a_fire); // mesh A input accepted this cycle
  Input(bit, mesh_b_fire); // mesh B input accepted this cycle
  Input(bit, mesh_d_fire); // mesh D input accepted this cycle
  Input(bit, mesh_a_rdy);
  Input(bit, mesh_b_rdy);
  Input(bit, mesh_d_rdy);
  Input(bit, mesh_req_rdy);

  Output(bit, firing); // any A/B/D row-feed stream is active
  Output(bit, a_fire); // A row-beat handshake
  Output(bit, b_fire); // B row-beat handshake
  Output(bit, d_fire); // D row-beat handshake
  Output(bit, mesh_cntl_deq_rdy); // mesh cntl queue can release current entry

  void update();
};

} // namespace smesh
