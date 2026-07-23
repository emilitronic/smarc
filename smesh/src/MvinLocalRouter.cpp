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
  UPDATE(updateDeqView)
      .reads(data_in)
      .writes(dmaread_spad_val,
              dmaread_spad_bits,
              dmaread_accum_val,
              dmaread_accum_bits);
  UPDATE(update)
      .reads(data_in, dmaread_spad_rdy, dmaread_accum_rdy)
      .writes(dmaread_spad, dmaread_accum);
}

void MvinLocalRouter::updateDeqView() {
  dmaread_spad_val = 0;
  dmaread_spad_bits = DmaReadResp{};
  dmaread_accum_val = 0;
  dmaread_accum_bits = DmaReadResp{};

  if (data_in.empty()) {
    return;
  }

  const auto& pending = data_in.peek();
  if (pending.laddr.is_acc_addr()) {
    dmaread_accum_val = 1;
    dmaread_accum_bits = pending;
  } else {
    dmaread_spad_val = 1;
    dmaread_spad_bits = pending;
  }
}

void MvinLocalRouter::update() {
  if (data_in.empty()) {
    return;
  }

  const auto& pending = data_in.peek();
  if (pending.laddr.is_acc_addr()) { // if data from DMA destined for accum...
    if (dmaread_accum_rdy != 0) {
      const auto data = data_in.pop();
      trace("mvin_local_router: deq accum laddr=0x%x cmd_id=%u",
            static_cast<unsigned>(data.laddr.raw),
            static_cast<unsigned>(data.cmd_id));
      return;
    }
    if (dmaread_accum.full()) {
      return;
    }
    const auto data = data_in.pop();
    dmaread_accum.push(data);
    trace("mvin_local_router: to accum laddr=0x%x cmd_id=%u",
          static_cast<unsigned>(data.laddr.raw),
          static_cast<unsigned>(data.cmd_id));
  } else {                           // if data from DMA destined for spad...
    if (dmaread_spad_rdy != 0) {
      const auto data = data_in.pop();
      trace("mvin_local_router: deq spad laddr=0x%x cmd_id=%u",
            static_cast<unsigned>(data.laddr.raw),
            static_cast<unsigned>(data.cmd_id));
      return;
    }
    if (dmaread_spad.full()) {
      return;
    }
    const auto data = data_in.pop();
    dmaread_spad.push(data);
    trace("mvin_local_router: to spad laddr=0x%x cmd_id=%u",
          static_cast<unsigned>(data.laddr.raw),
          static_cast<unsigned>(data.cmd_id));
  }
}

} // namespace smesh
