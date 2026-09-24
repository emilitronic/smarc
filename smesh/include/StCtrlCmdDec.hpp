// **********************************************************************
// smesh/include/StCtrlCmdDec.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
/*
Store-controller head-command decoder.

This block only reinterprets the current command-window head.  It owns no
configuration registers, counters, address progression, or handshakes.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StCtrlCmdDec : public Component {
  DECLARE_COMPONENT(StCtrlCmdDec);

 public:
  StCtrlCmdDec(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, head_val);
  Input(SmeshIssue, head_bits);

  Output(bit, do_config);
  Output(bit, do_config_norm);
  Output(bit, do_store);
  Output(bit, dst_is_spad);

  Output(u64, vaddr);
  Output(SmeshLocalAddr, dst_spad_addr);
  Output(u32, dst_spad_stride);
  Output(SmeshLocalAddr, localaddr);
  Output(u32, rows);
  Output(u32, cols);
  Output(u32, blocks);

  Output(u8, config_cmd_type); // for observation only
  Output(u32, config_stride);
  Output(u8, config_activation);
  Output(u32, config_acc_scale);
  Output(u8, config_pool_stride);
  Output(u8, config_pool_size);
  Output(u8, config_pool_out_dim);
  Output(u8, config_porows);
  Output(u8, config_pocols);
  Output(u8, config_orows);
  Output(u8, config_ocols);
  Output(u8, config_upad);
  Output(u8, config_lpad);

  Output(u8, config_stats_id);
  Output(bit, config_activation_msb);
  Output(bit, config_set_stats_id_only);
  Output(bit, config_iexp_q_const_type);
  Output(u32, config_iexp_q_const);
  Output(u32, config_igelu_qb);
  Output(u32, config_igelu_qc);

  Output(u32, mstatus);

  void update();
};

} // namespace smesh
