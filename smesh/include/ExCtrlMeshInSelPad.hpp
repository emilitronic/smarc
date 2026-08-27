// **********************************************************************
// smesh/include/ExCtrlMeshInSelPad.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Skeleton mesh input selection and padding for ExecuteController A/B/D feeds.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlMeshCntlQueue.hpp"
#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

class ExCtrlMeshInSelPad : public Component {
  DECLARE_COMPONENT(ExCtrlMeshInSelPad);

 public:
  ExCtrlMeshInSelPad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, cntl_val);
  Input(ExCtrlMeshCntl, cntl_bits);
  Input(u64, im2col_data);
  Input(bit, im2col_val);
  Input(bit, mesh_a_rdy); 
  Input(bit, mesh_b_rdy);
  Input(bit, mesh_d_rdy);
  InputArray(bit, spad_read_resp_val, kSpBanks);
  InputArray(bit, accum_read_resp_val, kAccBanks);
  InputArray(SpadReadResp, spad_read_resp_data, kSpBanks);
  InputArray(ExCtrlAccumReadResp, accum_read_resp_data, kAccBanks);
  OutputArray(bit, spad_read_resp_rdy, kSpBanks);
  OutputArray(bit, accum_read_resp_rdy, kAccBanks);

  Output(ExCtrlMeshIn, mesh_a);
  Output(ExCtrlMeshIn, mesh_b);
  Output(ExCtrlMeshIn, mesh_d);
  Output(bit, mesh_a_val);
  Output(bit, mesh_b_val);
  Output(bit, mesh_d_val);
  Output(bit, mesh_a_fire);
  Output(bit, mesh_b_fire);
  Output(bit, mesh_d_fire);

  void update();
};

} // namespace smesh
