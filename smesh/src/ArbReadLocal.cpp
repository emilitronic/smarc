// **********************************************************************
// smesh/src/ArbReadLocal.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 15 2026
/*
Local-memory read arbiter implementations.  Helps connect Ld/St/ExCtrl to local memory ports.
*/

#include "ArbReadLocal.hpp"

namespace smesh {

ArbReadSpad::ArbReadSpad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(exread_val, exread_bits, dmawrite_val, dmawrite_bits, read_req_rdy)
      .writes(exread_rdy, dmawrite_rdy, read_req_val, read_req_bits);
}

void ArbReadSpad::update() {
  const bool exread   = exread_val   != 0; // ExCtrl is asking to read spad this cycle
  const bool dmawrite = dmawrite_val != 0; // store path asking to read spad this cycle

  read_req_val  = bit(exread || dmawrite); // if either Ex or St path wants to read spad, send valid
  read_req_bits = exread ? *exread_bits : *dmawrite_bits; // choose request payload to put in spad

  exread_rdy   = bit(exread && read_req_rdy != 0);
  dmawrite_rdy = bit(!exread && dmawrite && read_req_rdy != 0);
}

ArbReadAccum::ArbReadAccum(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(exread_val, dmawrite_val, dmawrite_bits, read_req_rdy)
      .writes(exread_rdy, dmawrite_rdy, read_req_val, read_req_bits);
}

void ArbReadAccum::update() {
  const bool exread   = exread_val   != 0;
  const bool dmawrite = dmawrite_val != 0;

  read_req_val  = bit(exread || dmawrite);
  // TODO: add exread_bits to this update's reads() list before ExCtrl can assert exread_val.
  read_req_bits = exread ? *exread_bits : *dmawrite_bits;

  exread_rdy  = bit(exread && read_req_rdy != 0);
  dmawrite_rdy = bit(!exread && dmawrite && read_req_rdy != 0);
}

} // namespace smesh
