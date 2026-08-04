// **********************************************************************
// smesh/include/ExCtrlMeshCntlPack.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
/*
Combinational packaging for ExecuteController mesh-control metadata.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlMeshCntlQueue.hpp"
#include "SmeshLocalAddr.hpp"
#include "SmeshPorts.hpp"

namespace smesh {

class ExCtrlMeshCntlPack : public Component {
  DECLARE_COMPONENT(ExCtrlMeshCntlPack);

 public:
  ExCtrlMeshCntlPack(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, perform_mul_pre);
  Input(bit, perform_single_mul);
  Input(bit, perform_single_preload);
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
  Input(bit, a_fire);
  Input(bit, b_fire);
  Input(bit, d_fire);
  Input(u32, a_unpadded_cols);
  Input(u32, b_unpadded_cols);
  Input(u32, d_unpadded_cols);
  Input(SmeshLocalAddr, c_addr);
  Input(u32, c_rows);
  Input(u32, c_cols);
  Input(bit, a_transpose);
  Input(bit, bd_transpose);
  Input(u32, total_rows);
  Input(bit, rs_tag_valid);
  Input(SmeshRsTag, rs_tag);
  Input(u8, dataflow);
  Input(bit, prop);
  Input(u32, shift);
  Input(bit, im2colling);
  Input(bit, first);

  Output(ExCtrlMeshCntl, cntl);

  void update();
};

} // namespace smesh
