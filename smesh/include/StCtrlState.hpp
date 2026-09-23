// **********************************************************************
// smesh/include/StCtrlState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
// Store command FSM, configuration registers, and request-position counters.
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

enum class StCtrlFsmState : std::uint8_t {
  WaitingForCommand,
  WaitingForDmaReqReady,
  SendingRows,
  Pooling
};

struct StCtrlStateRegs {
  std::uint8_t state = static_cast<std::uint8_t>(StCtrlFsmState::WaitingForCommand);
  std::uint16_t cmd_id = 0;
  std::uint32_t stride = 0;
  std::uint8_t activation = 0;
  std::uint32_t acc_scale = 0;
  std::uint32_t igelu_qb = 0;
  std::uint32_t igelu_qc = 0;
  std::uint32_t iexp_qln2 = 0;
  std::uint32_t iexp_qln2_inv = 0;
  std::uint16_t norm_stats_id = 0;
  std::uint8_t pool_stride = 0;
  std::uint8_t pool_size = 0;
  std::uint8_t pool_out_dim = 0;
  std::uint8_t pool_porows = 0;
  std::uint8_t pool_pocols = 0;
  std::uint8_t pool_orows = 0;
  std::uint8_t pool_ocols = 0;
  std::uint8_t pool_upad = 0;
  std::uint8_t pool_lpad = 0;
  std::uint32_t row_counter = 0;
  std::uint32_t block_counter = 0;
  std::uint32_t porow_counter = 0;
  std::uint32_t pocol_counter = 0;
  std::uint32_t wrow_counter = 0;
  std::uint32_t wcol_counter = 0;
};

class StCtrlState : public Component {
  DECLARE_COMPONENT(StCtrlState);

 public:
  StCtrlState(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, head_val);
  Input(SmeshIssue, head_bits);
  Input(bit, do_config);
  Input(bit, do_config_norm);
  Input(bit, do_store);
  Input(u32, rows);
  Input(u32, blocks);
  Input(SmeshLocalAddr, localaddr);
  Input(u32, mvout_1d_rows);
  Input(u32, pool_total_rows);
  Input(bit, pooling_is_enabled);
  Input(bit, mvout_1d_enabled);
  Input(bit, tracker_alloc_rdy);
  Input(u16, tracker_alloc_cmd_id);
  Input(bit, dma_req_rdy);

  Input(u32, config_stride);
  Input(u8,  config_activation);
  Input(u32, config_acc_scale);
  Input(u8,  config_pool_stride);
  Input(u8,  config_pool_size);
  Input(u8,  config_pool_out_dim);
  Input(u8,  config_porows);
  Input(u8,  config_pocols);
  Input(u8,  config_orows);
  Input(u8,  config_ocols);
  Input(u8,  config_upad);
  Input(u8,  config_lpad);
  Input(u8,  config_stats_id);
  Input(bit, config_activation_msb);
  Input(bit, config_set_stats_id_only);
  Input(bit, config_iexp_q_const_type);
  Input(u32, config_iexp_q_const);
  Input(u32, config_igelu_qb);
  Input(u32, config_igelu_qc);

  Output(u8,  control_state);
  Output(u32, stride);
  Output(u8,  activation);
  Output(u32, acc_scale);
  Output(u32, igelu_qb);
  Output(u32, igelu_qc);
  Output(u32, iexp_qln2);
  Output(u32, iexp_qln2_inv);
  Output(u16, norm_stats_id);
  Output(u8,  pool_stride);
  Output(u8,  pool_size);
  Output(u8,  pool_out_dim);
  Output(u8,  pool_porows);
  Output(u8,  pool_pocols);
  Output(u8,  pool_orows);
  Output(u8,  pool_ocols);
  Output(u8,  pool_upad);
  Output(u8,  pool_lpad);
  Output(u32, row_counter);
  Output(u32, block_counter);
  Output(u32, porow_counter);
  Output(u32, pocol_counter);
  Output(u32, wrow_counter);
  Output(u32, wcol_counter);

  Output(bit, tracker_alloc_val);
  Output(u32, tracker_alloc_response_count);
  Output(SmeshRsTag, tracker_alloc_rs_tag);
  Output(bit, dma_req_val);
  Output(u16, dma_req_cmd_id);
  Output(bit, head_rdy);

  void updateConfigView();
  void updateRequest();
  void updateTransition();
  void reset();

 private:
  Output(StCtrlStateRegs,   regs_Q_);
  Register(StCtrlStateRegs, regs_D_);
};

} // namespace smesh
