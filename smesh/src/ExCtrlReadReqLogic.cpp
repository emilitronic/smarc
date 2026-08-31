// **********************************************************************
// smesh/src/ExCtrlReadReqLogic.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlReadReqLogic.hpp"

namespace smesh {

TraceKey(ex_ctrl_read_req_view);

ExCtrlReadReqLogic::ExCtrlReadReqLogic(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(start_inputting_a, start_inputting_b, start_inputting_d,
             a_address, b_address, d_address,
             a_valid,
             b_valid)
      .reads(d_valid,
             a_row_is_not_all_zeros, b_row_is_not_all_zeros, d_row_is_not_all_zeros,
             multiply_garbage,
             accumulate_zeros,
             preload_zeros,
             a_read_from_acc)
      .reads(b_read_from_acc,
             d_read_from_acc,
             dataAbank, dataBbank, dataDbank,
             dataABankAcc, dataBBankAcc, dataDBankAcc)
      .reads(spad_read_req_rdy,
             accum_read_req_rdy,
             cntl_rdy,
             acc_scale,
             activation,
             im2col_wire,
             im2col_en)
      .writes(a_ready, b_ready, d_ready,
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
  const auto a_addr = *a_address;
  const auto b_addr = *b_address;
  const auto d_addr = *d_address;
  const bool a_uses_im2col = im2col_wire != 0 && im2col_en != 0;

  // Operands that need no memory access are ready immediately. A real read
  // lowers the corresponding ready signal only when its selected bank stalls.
  bool next_a_ready = true;
  bool next_b_ready = true;
  bool next_d_ready = true;

  // *** SPAD READ REQUESTS ***
  // Convert the arbitrated A/B/D candidates into per-bank scratchpad reads.
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    // read_a/b/d: should ExCtrl issue read request on this bank in this cycle?
    const bool read_a = a_valid != 0 && a_read_from_acc == 0 && dataAbank == bank &&
                        start_inputting_a != 0 && multiply_garbage == 0 &&
                        a_row_is_not_all_zeros != 0 && !a_uses_im2col;
    const bool read_b = b_valid != 0 && b_read_from_acc == 0 && dataBbank == bank &&
                        start_inputting_b != 0 && accumulate_zeros == 0 &&
                        b_row_is_not_all_zeros != 0;
    const bool read_d = d_valid != 0 && d_read_from_acc == 0 && dataDbank == bank &&
                        start_inputting_d != 0 && preload_zeros == 0 &&
                        d_row_is_not_all_zeros != 0;
    // backpressure into A/B/D ready: if need to read A/B/D, but bank not ready, then A/B/D not ready
    if (read_a && spad_read_req_rdy[bank] == 0) { next_a_ready = false; }
    if (read_b && spad_read_req_rdy[bank] == 0) { next_b_ready = false; }
    if (read_d && spad_read_req_rdy[bank] == 0) { next_d_ready = false; }

    // Upstream priority logic normally makes these candidates exclusive.
    const auto selected_addr     = read_b ? b_addr : read_d ? d_addr : a_addr;
    spad_read_req_val[bank]      = bit((read_a || read_b || read_d) && cntl_rdy != 0);
    spad_read_req_addr[bank]     = selected_addr.sp_row();
    spad_read_req_from_dma[bank] = 0;
  }

  // *** ACCUMULATOR READ REQUESTS ***
  // Accumulator reads use the same operand conditions and bank-local address
  // selection, with the CONFIG_EX scale/activation settings attached.
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    const bool read_a = a_valid != 0 && a_read_from_acc != 0 && dataABankAcc == bank &&
                        start_inputting_a != 0 && multiply_garbage == 0 &&
                        a_row_is_not_all_zeros != 0 && !a_uses_im2col;
    const bool read_b = b_valid != 0 && b_read_from_acc != 0 && dataBBankAcc == bank &&
                        start_inputting_b != 0 && accumulate_zeros == 0 &&
                        b_row_is_not_all_zeros != 0;
    const bool read_d = d_valid != 0 && d_read_from_acc != 0 && dataDBankAcc == bank &&
                        start_inputting_d != 0 && preload_zeros == 0 &&
                        d_row_is_not_all_zeros != 0;

    if (read_a && accum_read_req_rdy[bank] == 0) { next_a_ready = false; }
    if (read_b && accum_read_req_rdy[bank] == 0) { next_b_ready = false; }
    if (read_d && accum_read_req_rdy[bank] == 0) { next_d_ready = false; }

    const auto selected_addr = read_b ? b_addr : read_d ? d_addr : a_addr;
    accum_read_req_val[bank]           = bit(read_a || read_b || read_d);
    accum_read_req_addr[bank]          = selected_addr.acc_row();
    accum_read_req_scale[bank]         = acc_scale;
    accum_read_req_full[bank]          = 0;
    accum_read_req_act[bank]           = activation;
    accum_read_req_igelu_qb[bank]      = 0;
    accum_read_req_igelu_qc[bank]      = 0;
    accum_read_req_iexp_qln2[bank]     = 0;
    accum_read_req_iexp_qln2_inv[bank] = 0;
    accum_read_req_from_dma[bank]      = 0;
  }

  a_ready = bit(next_a_ready);
  b_ready = bit(next_b_ready);
  d_ready = bit(next_d_ready);

  unsigned spad_valid_mask  = 0; // bitmask of which spad banks are being read this cycle, e.g. 0x2 = 0010
  unsigned accum_valid_mask = 0;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_valid_mask |= static_cast<unsigned>(spad_read_req_val[bank] != 0) << bank;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_valid_mask |= static_cast<unsigned>(accum_read_req_val[bank] != 0) << bank;
  }
  trace(ex_ctrl_read_req_view,
        "start{%u%u%u} allow{%u%u%u} ready{%u%u%u} sp_mask=0x%x acc_mask=0x%x cntl_rdy=%u\n",
        static_cast<unsigned>(start_inputting_a != 0),
        static_cast<unsigned>(start_inputting_b != 0),
        static_cast<unsigned>(start_inputting_d != 0),
        static_cast<unsigned>(a_valid != 0),
        static_cast<unsigned>(b_valid != 0),
        static_cast<unsigned>(d_valid != 0),
        static_cast<unsigned>(next_a_ready),
        static_cast<unsigned>(next_b_ready),
        static_cast<unsigned>(next_d_ready),
        spad_valid_mask,
        accum_valid_mask,
        static_cast<unsigned>(cntl_rdy != 0));
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (spad_read_req_val[bank] != 0) {
      trace(ex_ctrl_read_req_view,
            "  spad[%u] row=%u ready=%u from_dma=%u\n",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(*spad_read_req_addr[bank]),
            static_cast<unsigned>(spad_read_req_rdy[bank] != 0),
            static_cast<unsigned>(spad_read_req_from_dma[bank] != 0));
    }
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (accum_read_req_val[bank] != 0) {
      trace(ex_ctrl_read_req_view,
            "  accum[%u] row=%u ready=%u scale=0x%x act=%u from_dma=%u\n",
            static_cast<unsigned>(bank),
            static_cast<unsigned>(*accum_read_req_addr[bank]),
            static_cast<unsigned>(accum_read_req_rdy[bank] != 0),
            static_cast<unsigned>(*accum_read_req_scale[bank]),
            static_cast<unsigned>(*accum_read_req_act[bank]),
            static_cast<unsigned>(accum_read_req_from_dma[bank] != 0));
    }
  }
}

} // namespace smesh
