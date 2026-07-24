// **********************************************************************
// smesh/src/ArbWriteLocal.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 23 2026
/*
Local-memory write arbiter implementations.
*/

#include "ArbWriteLocal.hpp"

namespace smesh {

ArbWriteSpad::ArbWriteSpad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady)
      .reads(write_rdy)
      .writes(exwrite_rdy, dmaread_rdy, zerowrite_rdy);
  UPDATE(updateWrite)
      .reads(exwrite_val,
             exwrite_bits,
             dmaread_val,
             dmaread_bits,
             zerowrite_val,
             zerowrite_bits)
      .writes(write_val, write_bits);
}

void ArbWriteSpad::updateReady() {
  // TODO: once multiple write sources can be active, refine source-ready
  // backpressure to account for priority without creating valid/ready loops.
  exwrite_rdy   = bit(write_rdy != 0);
  dmaread_rdy   = bit(write_rdy != 0);
  zerowrite_rdy = bit(write_rdy != 0);
}

void ArbWriteSpad::updateWrite() {
  const bool exwrite   = exwrite_val   != 0;
  const bool dmaread   = dmaread_val   != 0;
  const bool zerowrite = zerowrite_val != 0;

  write_val = bit(exwrite || dmaread || zerowrite);
  if (exwrite) {
    write_bits = *exwrite_bits;
  } else if (dmaread) {
    write_bits = *dmaread_bits;
  } else {
    write_bits = *zerowrite_bits;
  }

}

void ArbWriteSpad::reset() {
  exwrite_rdy.reset(1);
  dmaread_rdy.reset(1);
  zerowrite_rdy.reset(1);
  write_val.reset(0);
  write_bits.reset(DmaReadResp{});
}

ArbWriteAccum::ArbWriteAccum(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady)
      .reads(write_rdy)
      .writes(exwrite_rdy, dmaread_full_rdy, dmaread_rdy, zerowrite_rdy);
  UPDATE(updateWrite)
      .reads(exwrite_val,
             exwrite_bits,
             dmaread_full_val,
             dmaread_full_bits,
             dmaread_val,
             dmaread_bits,
             zerowrite_val,
             zerowrite_bits)
      .writes(write_val, write_bits);
}

void ArbWriteAccum::updateReady() {
  // TODO: once multiple write sources can be active, refine source-ready
  // backpressure to account for priority without creating valid/ready loops.
  exwrite_rdy = bit(write_rdy != 0);
  dmaread_full_rdy = bit(write_rdy != 0);
  dmaread_rdy = bit(write_rdy != 0);
  zerowrite_rdy = bit(write_rdy != 0);
}

void ArbWriteAccum::updateWrite() {
  const bool exwrite      = exwrite_val      != 0;
  const bool dmaread_full = dmaread_full_val != 0;
  const bool dmaread      = dmaread_val      != 0;
  const bool zerowrite    = zerowrite_val    != 0;

  write_val = bit(exwrite || dmaread_full || dmaread || zerowrite);
  if (exwrite) {
    write_bits = *exwrite_bits;
  } else if (dmaread_full) {
    write_bits = *dmaread_full_bits;
  } else if (dmaread) {
    write_bits = *dmaread_bits;
  } else {
    write_bits = *zerowrite_bits;
  }

}

void ArbWriteAccum::reset() {
  exwrite_rdy.reset(1);
  dmaread_full_rdy.reset(1);
  dmaread_rdy.reset(1);
  zerowrite_rdy.reset(1);
  write_val.reset(0);
  write_bits.reset(DmaReadResp{});
}

} // namespace smesh
