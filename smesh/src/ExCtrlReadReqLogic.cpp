// **********************************************************************
// smesh/src/ExCtrlReadReqLogic.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlReadReqLogic.hpp"

namespace smesh {

ExCtrlReadReqLogic::ExCtrlReadReqLogic(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(start_inputting_a,
             start_inputting_b,
             start_inputting_d,
             a_address,
             b_address,
             d_address,
             a_valid,
             b_valid)
      .reads(d_valid,
             a_row_is_not_all_zeros,
             b_row_is_not_all_zeros,
             d_row_is_not_all_zeros,
             multiply_garbage,
             accumulate_zeros,
             preload_zeros,
             a_read_from_acc)
      .reads(b_read_from_acc,
             d_read_from_acc,
             dataAbank,
             dataBbank,
             dataDbank,
             dataABankAcc,
             dataBBankAcc,
             dataDBankAcc)
      .reads(spad_read_req_rdy,
             accum_read_req_rdy,
             cntl_rdy)
      .writes(a_ready,
              b_ready,
              d_ready,
              spad_read_req_val,
              spad_read_req_addr,
              spad_read_req_from_dma,
              accum_read_req_val,
              accum_read_req_addr)
      .writes(
              accum_read_req_scale)
      .writes(accum_read_req_full,
              accum_read_req_act,
              accum_read_req_igelu_qb,
              accum_read_req_igelu_qc,
              accum_read_req_iexp_qln2,
              accum_read_req_iexp_qln2_inv,
              accum_read_req_from_dma);
}

void ExCtrlReadReqLogic::update() {
  a_ready = 0;
  b_ready = 0;
  d_ready = 0;

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_read_req_val[bank] = 0;
    spad_read_req_addr[bank] = SmeshLocalAddr{};
    spad_read_req_from_dma[bank] = 0;
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_req_val[bank] = 0;
    accum_read_req_addr[bank] = SmeshLocalAddr{};
    accum_read_req_scale[bank] = 0;
    accum_read_req_full[bank] = 0;
    accum_read_req_act[bank] = 0;
    accum_read_req_igelu_qb[bank] = 0;
    accum_read_req_igelu_qc[bank] = 0;
    accum_read_req_iexp_qln2[bank] = 0;
    accum_read_req_iexp_qln2_inv[bank] = 0;
    accum_read_req_from_dma[bank] = 0;
  }
}

} // namespace smesh
