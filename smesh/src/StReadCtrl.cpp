// **********************************************************************
// smesh/src/StReadCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 11 2026
/*
Store-path local-memory read control implementation.
*/

#include "StReadCtrl.hpp"

namespace smesh {

StReadCtrl::StReadCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReadReq)
      .reads(dispatch_val, dispatch_bits, norm_rdy, dma_resp)
      .writes(dmawrite_spad, dmawrite_accum, spad_req_bits, accum_req_bits);
  UPDATE(updateReadFire)
      .reads(dispatch_val, dispatch_bits, norm_rdy, spad_read_req_rdy, accum_read_req_rdy, dma_resp)
      .writes(read_req_fire, dma_resp);
  UPDATE(updateInspect).reads(dispatch_val, dispatch_bits, norm_rdy, spad_read_req_rdy, accum_read_req_rdy);
}
// do I want a store-side local-memory read? if yes, which memory? what requests bits to present?
void StReadCtrl::updateReadReq() {
  const auto req = *dispatch_bits;
  const auto laddr = req.laddr;
  const bool is_live = !laddr.is_garbage();
  const bool resp_ready = !dma_resp.full();
  const bool spad_valid = dispatch_val != 0 &&    // if dispatch queue has a command...
                          is_live &&              // ...and local address is not garbage...
                          !laddr.is_acc_addr() && // ...and local address is not an accum address...
                          norm_rdy != 0 &&        // ...and norm queue is ready...
                          resp_ready;             // ...and StCtrl can receive the accept response
  const bool accum_valid = dispatch_val != 0 &&
                           is_live &&
                           laddr.is_acc_addr() &&
                           norm_rdy != 0 &&
                           resp_ready;
  const auto spad_bank = laddr.sp_bank();
  const auto acc_bank = laddr.acc_bank();

  // tap off dispatch_bits for spat read req payload
  SpadReadReq spad_req{};
  spad_req.laddr = laddr;
  spad_req.len = req.len;
  spad_req.cmd_id = req.cmd_id;
  spad_req.from_dma = true;
  // tap off dispatch_bits for accum read req payload
  AccumReadReq accum_req{};
  accum_req.laddr = laddr;
  accum_req.len = req.len;
  accum_req.act = req.acc_act;
  accum_req.scale = req.acc_scale;
  accum_req.igelu_qb = req.acc_igelu_qb;
  accum_req.igelu_qc = req.acc_igelu_qc;
  accum_req.iexp_qln2 = req.acc_iexp_qln2;
  accum_req.iexp_qln2_inv = req.acc_iexp_qln2_inv;
  accum_req.full = laddr.read_full_acc_row();
  accum_req.cmd_id = req.cmd_id;
  accum_req.from_dma = true;

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    dmawrite_spad[bank] = bit(spad_valid && bank == spad_bank);
    spad_req_bits[bank] = spad_req;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    dmawrite_accum[bank] = bit(accum_valid && bank == acc_bank);
    accum_req_bits[bank] = accum_req;
  }
}
// compute whether store-read action can advance this cycle
void StReadCtrl::updateReadFire() {
  const auto req = *dispatch_bits;
  const auto laddr = req.laddr;
  const bool is_live = !laddr.is_garbage();
  const bool resp_ready = !dma_resp.full();
  const bool garbage_fire = dispatch_val != 0 &&
                            laddr.is_garbage() &&
                            norm_rdy != 0 &&
                            resp_ready;
  const bool spad_valid = dispatch_val != 0 &&
                          is_live &&
                          !laddr.is_acc_addr() &&
                          norm_rdy != 0 &&
                          resp_ready;
  const bool accum_valid = dispatch_val != 0 &&
                           is_live &&
                           laddr.is_acc_addr() &&
                           norm_rdy != 0 &&
                           resp_ready;
  const bool read_fire = garbage_fire ||
                         (spad_valid && spad_read_req_rdy[laddr.sp_bank()] != 0) ||
                         (accum_valid && accum_read_req_rdy[laddr.acc_bank()] != 0);
  // output
  read_req_fire  = bit(read_fire);
  if (read_fire) {
    DmaWriteResp response{};
    response.cmd_id = req.cmd_id;
    dma_resp.push(response);
  }
}
// look at dispatch queue command
void StReadCtrl::updateInspect() {
  if (dispatch_val == 0) {
    return;
  }

  const auto req = *dispatch_bits;
  const auto laddr = req.laddr;
  const bool is_live = !laddr.is_garbage();
  const bool targets_spad = is_live && !laddr.is_acc_addr();
  const bool targets_accum = is_live && laddr.is_acc_addr();
  const bool dmawrite_spad = dispatch_val != 0 && targets_spad &&
                             norm_rdy != 0;
  const bool dmawrite_accum = dispatch_val != 0 && targets_accum &&
                              norm_rdy != 0;
  const bool dmawrite = dmawrite_spad || dmawrite_accum;
  const bool read_fire = (dmawrite_spad && spad_read_req_rdy[laddr.sp_bank()] != 0) ||
                         (dmawrite_accum && accum_read_req_rdy[laddr.acc_bank()] != 0);

  trace("st_read_ctrl: laddr=0x%x spad=%u sp_bank=%u spad_ready=%u accum=%u acc_bank=%u accum_ready=%u norm_rdy=%u dmawrite_spad=%u dmawrite_accum=%u dmawrite=%u read_fire=%u len=%u cmd_id=%u",
        static_cast<unsigned>(laddr.raw),
        static_cast<unsigned>(targets_spad),
        static_cast<unsigned>(laddr.sp_bank()),
        static_cast<unsigned>(spad_read_req_rdy[laddr.sp_bank()]),
        static_cast<unsigned>(targets_accum),
        static_cast<unsigned>(laddr.acc_bank()),
        static_cast<unsigned>(accum_read_req_rdy[laddr.acc_bank()]),
        static_cast<unsigned>(norm_rdy),
        static_cast<unsigned>(dmawrite_spad),
        static_cast<unsigned>(dmawrite_accum),
        static_cast<unsigned>(dmawrite),
        static_cast<unsigned>(read_fire),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.cmd_id));
}

} // namespace smesh
