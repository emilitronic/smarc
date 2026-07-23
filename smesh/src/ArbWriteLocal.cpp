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
  UPDATE(update)
      .reads(exwrite_val,
             exwrite_bits,
             dmaread_val,
             dmaread_bits,
             zerowrite_val,
             zerowrite_bits,
             write_rdy)
      .writes(exwrite_rdy,
              dmaread_rdy,
              zerowrite_rdy,
              write_val,
              write_bits);
}

void ArbWriteSpad::update() {
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

  exwrite_rdy   = bit(exwrite && write_rdy != 0);
  dmaread_rdy   = bit(!exwrite && dmaread && write_rdy != 0);
  zerowrite_rdy = bit(!exwrite && !dmaread && zerowrite && write_rdy != 0);
}

ArbWriteAccum::ArbWriteAccum(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(exwrite_val,
             exwrite_bits,
             dmaread_full_val,
             dmaread_full_bits,
             dmaread_val,
             dmaread_bits,
             zerowrite_val,
             zerowrite_bits)
      .reads(write_rdy)
      .writes(exwrite_rdy,
              dmaread_full_rdy,
              dmaread_rdy,
              zerowrite_rdy,
              write_val,
              write_bits);
}

void ArbWriteAccum::update() {
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

  exwrite_rdy = bit(exwrite && write_rdy != 0);
  dmaread_full_rdy = bit(!exwrite && dmaread_full && write_rdy != 0);
  dmaread_rdy = bit(!exwrite && !dmaread_full && dmaread && write_rdy != 0);
  zerowrite_rdy = bit(!exwrite && !dmaread_full && !dmaread && zerowrite && write_rdy != 0);
}

} // namespace smesh
