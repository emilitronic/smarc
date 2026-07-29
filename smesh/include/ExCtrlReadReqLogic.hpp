// **********************************************************************
// smesh/include/ExCtrlReadReqLogic.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
/*
Combinational ExecuteController operand-read request generation.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

class ExCtrlReadReqLogic : public Component {
  DECLARE_COMPONENT(ExCtrlReadReqLogic);

 public:
  ExCtrlReadReqLogic(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, start_inputting_a);
  Input(bit, start_inputting_b);
  Input(bit, start_inputting_d);
  Input(SmeshLocalAddr, a_address);
  Input(SmeshLocalAddr, b_address);
  Input(SmeshLocalAddr, d_address);
  Input(bit, a_valid);
  Input(bit, b_valid);
  Input(bit, d_valid);
  Input(bit, a_row_is_not_all_zeros);
  Input(bit, b_row_is_not_all_zeros);
  Input(bit, d_row_is_not_all_zeros);
  Input(bit, multiply_garbage);
  Input(bit, accumulate_zeros);
  Input(bit, preload_zeros);
  Input(bit, a_read_from_acc);
  Input(bit, b_read_from_acc);
  Input(bit, d_read_from_acc);
  Input(u32, dataAbank);
  Input(u32, dataBbank);
  Input(u32, dataDbank);
  Input(u32, dataABankAcc);
  Input(u32, dataBBankAcc);
  Input(u32, dataDBankAcc);
  InputArray(bit, spad_read_req_rdy, kSpBanks);
  InputArray(bit, accum_read_req_rdy, kAccBanks);
  Input(bit, cntl_ready);

  Output(bit, a_ready);
  Output(bit, b_ready);
  Output(bit, d_ready);
  OutputArray(bit, spad_read_req_val, kSpBanks);
  OutputArray(SmeshLocalAddr, spad_read_req_addr, kSpBanks);
  OutputArray(bit, spad_read_req_from_dma, kSpBanks);
  OutputArray(bit, accum_read_req_val, kAccBanks);
  OutputArray(SmeshLocalAddr, accum_read_req_addr, kAccBanks);
  OutputArray(u32, accum_read_req_scale, kAccBanks);
  OutputArray(bit, accum_read_req_full, kAccBanks);
  OutputArray(u8, accum_read_req_act, kAccBanks);
  OutputArray(u32, accum_read_req_igelu_qb, kAccBanks);
  OutputArray(u32, accum_read_req_igelu_qc, kAccBanks);
  OutputArray(u32, accum_read_req_iexp_qln2, kAccBanks);
  OutputArray(u32, accum_read_req_iexp_qln2_inv, kAccBanks);
  OutputArray(bit, accum_read_req_from_dma, kAccBanks);

  void update();
};

} // namespace smesh
