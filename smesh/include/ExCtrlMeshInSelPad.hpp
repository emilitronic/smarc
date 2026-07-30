// **********************************************************************
// smesh/include/ExCtrlMeshInSelPad.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Skeleton mesh input selection and padding for ExecuteController A/B/D feeds.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

struct ExCtrlMeshInput {
  u64 data = 0;
  bit valid = 0;
};

class ExCtrlMeshInSelPad : public Component {
  DECLARE_COMPONENT(ExCtrlMeshInSelPad);

 public:
  ExCtrlMeshInSelPad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, cntl_val);
  Input(u32, a_bank);
  Input(u32, b_bank);
  Input(u32, d_bank);
  Input(u32, a_bank_acc);
  Input(u32, b_bank_acc);
  Input(u32, d_bank_acc);
  Input(bit, a_read_from_acc);
  Input(bit, b_read_from_acc);
  Input(bit, d_read_from_acc);
  Input(bit, a_garbage);
  Input(bit, b_garbage);
  Input(bit, d_garbage);
  Input(bit, accumulate_zeros);
  Input(bit, preload_zeros);
  Input(bit, im2colling);
  Input(u64, im2col_data);
  Input(bit, im2col_val);
  Input(u32, a_unpadded_cols);
  Input(u32, b_unpadded_cols);
  Input(u32, d_unpadded_cols);
  Input(bit, cntl_a_fire);
  Input(bit, cntl_b_fire);
  Input(bit, cntl_d_fire);
  Input(bit, mesh_a_rdy);
  Input(bit, mesh_b_rdy);
  Input(bit, mesh_d_rdy);
  InputArray(bit, spad_read_val, kSpBanks);
  InputArray(bit, accum_read_val, kAccBanks);
  InputArray(SpadReadResp, spad_read_data, kSpBanks);
  InputArray(AccumReadResp, accum_read_data, kAccBanks);

  Output(ExCtrlMeshInput, mesh_a);
  Output(ExCtrlMeshInput, mesh_b);
  Output(ExCtrlMeshInput, mesh_d);
  Output(bit, mesh_a_val);
  Output(bit, mesh_b_val);
  Output(bit, mesh_d_val);
  Output(bit, mesh_a_fire);
  Output(bit, mesh_b_fire);
  Output(bit, mesh_d_fire);

  void update();
};

} // namespace smesh
