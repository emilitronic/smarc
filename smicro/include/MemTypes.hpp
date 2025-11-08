// **********************************************************************
// smicro/include/MemTypes.hpp
// **********************************************************************
// S Magierowski Aug 16 2025

#pragma once
#include <cascade/Cascade.hpp>

// Minimal memory request/response packet types (FIFO-friendly)
// Team convention: tags and errors are explicit for routing/arbitration.
struct MemReq {
  u64  addr  = 0;     // byte address
  u64  wdata = 0;     // write data (single beat for now)
  u16  size  = 8;     // bytes per beat
  bit  write = false; // write=1 store, write=0 load
  u16  id    = 0;     // transaction id (master-provided)
};

struct MemResp {
  u64  rdata = 0;   // read data (single beat for now)
  u16  id    = 0;   // transaction id
  u8   err   = 0;   // 0=OK, nonzero=error code
};
