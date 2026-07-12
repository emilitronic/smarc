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
  UPDATE(updateReadFire).reads(dispatch_val, dispatch_bits, norm_rdy, spad_read_rdy, accum_read_rdy).writes(read_req_fire);
  UPDATE(updateInspect).reads(dispatch_val, dispatch_bits, norm_rdy, spad_read_rdy, accum_read_rdy);
}
// compute whether store-read action can advance this cycle
void StReadCtrl::updateReadFire() {
  const auto req = *dispatch_bits;
  const auto laddr = req.laddr;
  const bool is_live = !laddr.is_garbage();
  const bool dmawrite_spad = dispatch_val != 0 &&    // if dispatch queue has a command...
                             is_live &&              // ...and local address is not garbage...
                             !laddr.is_acc_addr() && // ...and local address is not an accum address...
                             norm_rdy != 0;          // ...and norm queue is ready, then this is a DMA write to spad
  const bool dmawrite_accum = dispatch_val != 0 && 
                              is_live && 
                              laddr.is_acc_addr() &&
                              norm_rdy != 0;
  const bool read_fire = (dmawrite_spad && spad_read_rdy != 0) ||
                         (dmawrite_accum && accum_read_rdy != 0);

  read_req_fire = bit(read_fire);
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
  const bool read_fire = (dmawrite_spad && spad_read_rdy != 0) ||
                         (dmawrite_accum && accum_read_rdy != 0);

  trace("st_read_ctrl: laddr=0x%x spad=%u sp_bank=%u spad_ready=%u accum=%u acc_bank=%u accum_ready=%u norm_rdy=%u dmawrite_spad=%u dmawrite_accum=%u dmawrite=%u read_fire=%u len=%u cmd_id=%u",
        static_cast<unsigned>(laddr.raw),
        static_cast<unsigned>(targets_spad),
        static_cast<unsigned>(laddr.sp_bank()),
        static_cast<unsigned>(spad_read_rdy),
        static_cast<unsigned>(targets_accum),
        static_cast<unsigned>(laddr.acc_bank()),
        static_cast<unsigned>(accum_read_rdy),
        static_cast<unsigned>(norm_rdy),
        static_cast<unsigned>(dmawrite_spad),
        static_cast<unsigned>(dmawrite_accum),
        static_cast<unsigned>(dmawrite),
        static_cast<unsigned>(read_fire),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.cmd_id));
}

} // namespace smesh
