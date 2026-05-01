// **********************************************************************
// smem/include/smem/MemTypes.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 16 2025
/*
Minimal memory request/response packet types (FIFO-friendly).  MemReq is a mem request ("read/write this addr"), and MemResp is mem response ("here's the read data or here's the store ack").  They travel through Cascade FIFO ports.
*/

#pragma once
#include <cascade/Cascade.hpp>

namespace smem {

struct MemReq {
  u64  addr  = 0;     // byte address
  u64  wdata = 0;     // write data (single beat for now)
  u16  size  = 8;     // bytes covered by memory op
  bit  write = false; // write=1 store, write=0 load
  u16  id    = 0;     // transaction id (requester can label so response can be matched to req)
};

struct MemResp {
  u64  rdata = 0;   // read data (single beat for now)
  u16  id    = 0;   // transaction id
  u8   err   = 0;   // 0=OK, nonzero=error code
};

} // namespace smem
