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
  UPDATE(updateReady).writes(read_req_fire);
  UPDATE(updateInspect).reads(dispatch_val, dispatch_bits, norm_rdy, spad_read_rdy, accum_read_rdy);
}

void StReadCtrl::updateReady() {
  read_req_fire = 1;
}

void StReadCtrl::updateInspect() {
  if (dispatch_val == 0) {
    return;
  }

  const auto req = *dispatch_bits;
  const auto laddr = req.laddr;
  const bool is_live = !laddr.is_garbage();
  const bool targets_spad = is_live && !laddr.is_acc_addr();
  const bool targets_accum = is_live && laddr.is_acc_addr();

  trace("st_read_ctrl: laddr=0x%x spad=%u sp_bank=%u spad_ready=%u accum=%u acc_bank=%u accum_ready=%u norm_rdy=%u len=%u cmd_id=%u",
        static_cast<unsigned>(laddr.raw),
        static_cast<unsigned>(targets_spad),
        static_cast<unsigned>(laddr.sp_bank()),
        static_cast<unsigned>(spad_read_rdy),
        static_cast<unsigned>(targets_accum),
        static_cast<unsigned>(laddr.acc_bank()),
        static_cast<unsigned>(accum_read_rdy),
        static_cast<unsigned>(norm_rdy),
        static_cast<unsigned>(req.len),
        static_cast<unsigned>(req.cmd_id));
}

} // namespace smesh
