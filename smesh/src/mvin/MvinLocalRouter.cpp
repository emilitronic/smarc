// **********************************************************************
// smesh/src/mvin/MvinLocalRouter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
Load-path local-memory router implementation.
*/

#include "MvinLocalRouter.hpp"

namespace smesh {

MvinLocalRouter::MvinLocalRouter(std::string /*name*/, IMPL_CTOR) {
  entry_Q_ <= entry_D_;
  UPDATE(updateView).reads(entry_Q_)
      .writes(dmaread_spad_val,
              dmaread_spad_bits,
              dmaread_accum_val,
              dmaread_accum_bits);
  UPDATE(updateReady)
      .reads(entry_Q_, dmaread_spad_rdy, dmaread_accum_rdy)
      .writes(in_rdy);
  UPDATE(updateStorage)
      .reads(entry_Q_, in_val, in_bits, in_rdy,
             dmaread_spad_rdy, dmaread_accum_rdy)
      .writes(entry_D_);
}

void MvinLocalRouter::updateView() {
  const auto entry = *entry_Q_;
  dmaread_spad_val = 0;
  dmaread_spad_bits = DmaReadResp{};
  dmaread_accum_val = 0;
  dmaread_accum_bits = DmaReadResp{};

  if (entry.valid == 0) return;
  const auto& pending = entry.bits;
  if (pending.laddr.is_acc_addr()) {
    dmaread_accum_val = 1;
    dmaread_accum_bits = pending;
  } else {                           // if data from DMA destined for spad...
    dmaread_spad_val = 1;
    dmaread_spad_bits = pending;
  }
}

void MvinLocalRouter::updateReady() {
  const auto entry = *entry_Q_;
  const bool output_ready = entry.bits.laddr.is_acc_addr()
                                ? dmaread_accum_rdy == 1
                                : dmaread_spad_rdy == 1;
  in_rdy = bit(entry.valid == 0 || output_ready);
}

void MvinLocalRouter::updateStorage() {
  const auto current = *entry_Q_;
  const bool output_ready = current.bits.laddr.is_acc_addr()
                                ? dmaread_accum_rdy == 1
                                : dmaread_spad_rdy == 1;
  const bool pop = current.valid == 1 && output_ready;
  const bool push = in_val == 1 && in_rdy == 1;
  if (!pop && !push) return;

  Entry next = current;
  if (pop) {
    trace("mvin_local_router: accepted %s laddr=0x%x cmd_id=%u",
          current.bits.laddr.is_acc_addr() ? "accum" : "spad",
          static_cast<unsigned>(current.bits.laddr.raw),
          static_cast<unsigned>(current.bits.cmd_id));
    next = Entry{};
  }
  if (push) {
    next.valid = 1;
    next.bits = *in_bits;
  }
  entry_D_ = next;
}

void MvinLocalRouter::reset() {
  entry_D_.reset(Entry{});
  in_rdy.reset(0);
  dmaread_spad_val.reset(0);
  dmaread_spad_bits.reset(DmaReadResp{});
  dmaread_accum_val.reset(0);
  dmaread_accum_bits.reset(DmaReadResp{});
}

} // namespace smesh
