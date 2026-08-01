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

#include "ExCtrlMeshCntlQueue.hpp"
#include "ExCtrlMeshInSelPad.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

struct MesherResp {
  u64 data = 0;
  std::uint32_t total_rows = 0;
  ExCtrlMeshTag tag{};
  bit last = 0;
};

using MesherTag = ExCtrlMeshTag;

class Mesher : public Component {
  DECLARE_COMPONENT(Mesher);

 public:
  Mesher(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, req_val);
  Output(bit, req_rdy);
  Input(ExCtrlMeshReq, req_bits);

  Input(bit, a_val);
  Output(bit, a_rdy);
  Input(ExCtrlMeshInput, a_bits);

  Input(bit, b_val);
  Output(bit, b_rdy);
  Input(ExCtrlMeshInput, b_bits);

  Input(bit, d_val);
  Output(bit, d_rdy);
  Input(ExCtrlMeshInput, d_bits);

  Output(bit, resp_val);
  Output(MesherResp, resp_bits);

  OutputArray(MesherTag, tags_in_progress, kRsExecuteEntries);

  void update();
  void reset();
};

} // namespace smesh
