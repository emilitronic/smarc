// **********************************************************************
// smesh/include/Mesher.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Systolic mesh boundary.

This starts as the structural boundary for the core mesh. It is not part of
ExCtrl proper: ExCtrl feeds it requests and row data, and a later writeback
block will consume its responses.
*/

#pragma once

#include <cstdint>

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

class Mesher : public Component {
  DECLARE_COMPONENT(Mesher);

 public:
  Mesher(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,           req_val);
  Output(bit,          req_rdy);
  Input(ExCtrlMeshReq, req_bits);

  Input(bit,          a_val);
  Output(bit,         a_rdy);
  Input(ExCtrlMeshIn, a_bits);

  Input(bit,          b_val);
  Output(bit,         b_rdy);
  Input(ExCtrlMeshIn, b_bits);

  Input(bit,          d_val);
  Output(bit,         d_rdy);
  Input(ExCtrlMeshIn, d_bits);

  Output(bit,        resp_val);
  Output(MesherResp, resp_bits);

  OutputArray(MesherTag, tags_in_progress, kRsExecuteEntries);

  void update();
  void reset();
};

} // namespace smesh
