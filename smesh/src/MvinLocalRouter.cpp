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
  UPDATE(update).reads(data_in).writes(dmaread_spad, dmaread_accum);
}

void MvinLocalRouter::update() {
  if (data_in.empty()) {
    return;
  }

  const auto& pending = data_in.peek();
  if (pending.laddr.is_acc_addr()) { // if data from DMA destined for accum...
    if (dmaread_accum.full()) {
      return;
    }
    const auto data = data_in.pop();
    dmaread_accum.push(data);
    trace("mvin_local_router: to accum laddr=0x%x cmd_id=%u",
          static_cast<unsigned>(data.laddr.raw),
          static_cast<unsigned>(data.cmd_id));
  } else {                           // if data from DMA destined for spad...
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
