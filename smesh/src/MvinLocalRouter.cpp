// **********************************************************************
// smesh/src/MvinLocalRouter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 9 2026
/*
Load-path local-memory router implementation.
*/

#include "MvinLocalRouter.hpp"

namespace smesh {

MvinLocalRouter::MvinLocalRouter(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(data_in, dmaread_spad_rdy, dmaread_accum_rdy)
      .writes(dmaread_spad, dmaread_accum);
  UPDATE(updateView)
      .writes(dmaread_spad_val,
              dmaread_spad_bits,
              dmaread_accum_val,
              dmaread_accum_bits);
}

void MvinLocalRouter::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  if (!entry_valid_) {
    if (data_in.empty()) {
      return;
    }
    entry_ = data_in.pop();
    entry_valid_ = true;
    return;
  }

  const auto& pending = entry_;
  if (pending.laddr.is_acc_addr()) { // if data from DMA destined for accum...
    if (dmaread_accum_rdy != 0) {
      trace("mvin_local_router: deq accum laddr=0x%x cmd_id=%u",
            static_cast<unsigned>(pending.laddr.raw),
            static_cast<unsigned>(pending.cmd_id));
      entry_ = DmaReadResp{};
      entry_valid_ = false;
      return;
    }
    if (dmaread_accum.full()) {
      return;
    }
    dmaread_accum.push(pending);
    trace("mvin_local_router: to accum laddr=0x%x cmd_id=%u",
          static_cast<unsigned>(pending.laddr.raw),
          static_cast<unsigned>(pending.cmd_id));
  } else {                           // if data from DMA destined for spad...
    if (dmaread_spad_rdy != 0) {
      trace("mvin_local_router: deq spad laddr=0x%x cmd_id=%u",
            static_cast<unsigned>(pending.laddr.raw),
            static_cast<unsigned>(pending.cmd_id));
      entry_ = DmaReadResp{};
      entry_valid_ = false;
      return;
    }
    if (dmaread_spad.full()) {
      return;
    }
    dmaread_spad.push(pending);
    trace("mvin_local_router: to spad laddr=0x%x cmd_id=%u",
          static_cast<unsigned>(pending.laddr.raw),
          static_cast<unsigned>(pending.cmd_id));
  }
  entry_ = DmaReadResp{};
  entry_valid_ = false;
}

void MvinLocalRouter::updateView() {
  dmaread_spad_val = 0;
  dmaread_spad_bits = DmaReadResp{};
  dmaread_accum_val = 0;
  dmaread_accum_bits = DmaReadResp{};

  if (!entry_valid_) {
    return;
  }

  const auto& pending = entry_;
  if (pending.laddr.is_acc_addr()) { // if data from DMA destined for accum...
    dmaread_accum_val = 1;
    dmaread_accum_bits = pending;
  } else {                           // if data from DMA destined for spad...
    dmaread_spad_val = 1;
    dmaread_spad_bits = pending;
  }
}

void MvinLocalRouter::reset() {
  entry_valid_ = false;
  entry_ = DmaReadResp{};
  dmaread_spad_val.reset(0);
  dmaread_spad_bits.reset(DmaReadResp{});
  dmaread_accum_val.reset(0);
  dmaread_accum_bits.reset(DmaReadResp{});
}

} // namespace smesh
