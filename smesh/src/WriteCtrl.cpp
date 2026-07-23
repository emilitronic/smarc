// **********************************************************************
// smesh/src/WriteCtrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 23 2026
/*
Load-return local-memory write control implementation.
*/

#include "WriteCtrl.hpp"

namespace smesh {

WriteCtrl::WriteCtrl(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(dmaread_spad_val,
             dmaread_spad_bits,
             dmaread_accum_val,
             dmaread_accum_bits,
             dmaread_accum_full_val,
             dmaread_accum_full_bits,
             arb_spad_dmaread_rdy,
             arb_accum_dmaread_rdy)
      .reads(arb_accum_dmaread_full_rdy)
      .writes(dmaread_spad_rdy,
              dmaread_accum_rdy,
              dmaread_accum_full_rdy,
              arb_spad_dmaread_val,
              arb_spad_dmaread_bits,
              arb_accum_dmaread_val,
              arb_accum_dmaread_bits)
      .writes(arb_accum_dmaread_full_val,
              arb_accum_dmaread_full_bits);
}

void WriteCtrl::update() {
  dmaread_spad_rdy = 0;
  dmaread_accum_rdy = 0;
  dmaread_accum_full_rdy = 0;

  const auto spad_payload = *dmaread_spad_bits;
  const auto accum_payload = *dmaread_accum_bits;
  const auto accum_full_payload = *dmaread_accum_full_bits;
  const auto spad_bank = spad_payload.laddr.sp_bank();
  const auto accum_bank = accum_payload.laddr.acc_bank();
  const auto accum_full_bank = accum_full_payload.laddr.acc_bank();

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    const bool selected = dmaread_spad_val != 0 && bank == spad_bank;
    arb_spad_dmaread_val[bank] = bit(selected);
    arb_spad_dmaread_bits[bank] = spad_payload;
  }
  if (dmaread_spad_val != 0) {
    dmaread_spad_rdy = arb_spad_dmaread_rdy[spad_bank];
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    const bool selected = dmaread_accum_val != 0 && bank == accum_bank;
    arb_accum_dmaread_val[bank] = bit(selected);
    arb_accum_dmaread_bits[bank] = accum_payload;
  }
  if (dmaread_accum_val != 0) {
    dmaread_accum_rdy = arb_accum_dmaread_rdy[accum_bank];
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    const bool selected = dmaread_accum_full_val != 0 && bank == accum_full_bank;
    arb_accum_dmaread_full_val[bank] = bit(selected);
    arb_accum_dmaread_full_bits[bank] = accum_full_payload;
  }
  if (dmaread_accum_full_val != 0) {
    dmaread_accum_full_rdy = arb_accum_dmaread_full_rdy[accum_full_bank];
  }
}

} // namespace smesh
