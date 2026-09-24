// **********************************************************************
// smesh/include/StCtrlGeom.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
/*
Store-controller pooling and moveout geometry.
  It computes:

  - normal moveout addresses: current_vaddr, current_localaddr, and current_dst_spad_addr;
  - pooling and 1-D moveout enable conditions;
  - pooling coordinates and negative-padding flags;
  - garbage local addresses for padded pooling elements;
  - pool_vaddr, pool_total_rows, and mvout_1d_rows.

This block is purely combinational. Configuration registers and runtime
counters are owned by the store-controller state block and supplied here.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StCtrlGeom : public Component {
  DECLARE_COMPONENT(StCtrlGeom);

 public:
  StCtrlGeom(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(u64, vaddr);
  Input(SmeshLocalAddr, localaddr);
  Input(SmeshLocalAddr, dst_spad_addr);
  Input(u32, dst_spad_stride);

  Input(u32, stride);
  Input(u8, pool_stride);
  Input(u8, pool_size);
  Input(u8, pool_out_dim);
  Input(u8, pool_porows);
  Input(u8, pool_pocols);
  Input(u8, pool_orows);
  Input(u8, pool_ocols);
  Input(u8, pool_upad);
  Input(u8, pool_lpad);

  Input(u32, row_counter);
  Input(u32, block_counter);
  Input(u32, porow_counter);
  Input(u32, pocol_counter);
  Input(u32, wrow_counter);
  Input(u32, wcol_counter);

  Output(bit, pooling_is_enabled);
  Output(bit, mvout_1d_enabled);
  Output(u32, orow); // currently only for observation
  Output(u32, ocol); // currently only for observation
  Output(bit, orow_is_negative); // currently only for observation
  Output(bit, ocol_is_negative); // currently only for observation
  Output(u32, pool_total_rows);
  Output(u32, mvout_1d_rows);

  Output(u64, current_vaddr);
  Output(SmeshLocalAddr, current_localaddr);
  Output(u64, current_dst_spad_addr);
  Output(SmeshLocalAddr, pool_row_addr);
  Output(u64, pool_vaddr);

  void update();
  void reset();
};

} // namespace smesh
