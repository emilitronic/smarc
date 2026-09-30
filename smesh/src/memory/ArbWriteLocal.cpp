// **********************************************************************
// smesh/src/memory/ArbWriteLocal.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 23 2026
/*
Local-memory write arbiter implementations.
*/

#include "ArbWriteLocal.hpp"

namespace smesh {

ArbWriteSpad::ArbWriteSpad(std::string /*name*/, std::size_t bank, IMPL_CTOR)
    : bank_(bank) {
  UPDATE(updateReady)
      .reads(exwrite_val, dmaread_val, write_rdy)
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
  exwrite_rdy   = bit(write_rdy != 0);
  dmaread_rdy   = bit(exwrite_val == 0 && write_rdy != 0);
  zerowrite_rdy = bit(exwrite_val == 0 && dmaread_val == 0 && write_rdy != 0);
}

void ArbWriteSpad::updateWrite() {
  const bool exwrite   = exwrite_val   != 0;
  const bool dmaread   = dmaread_val   != 0;
  const bool zerowrite = zerowrite_val != 0;

  write_val = bit(exwrite || dmaread || zerowrite);
  if (exwrite) {
    const auto req = *exwrite_bits;
    DmaReadResp write{};
    // address = bank * rows_per_bank + row_within_bank 
    write.laddr = makeSpAddr(static_cast<std::uint32_t>(bank_ * kSpBankRows) + (req.addr & kSpBankRowMask)); // retaining & kSpBankRowMask as defensive measure, but correctly formed ExCtrl request should not need it
    // copy kDim elements from ExCtrl's request into byte-array format used by DmaReadResp
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      write.data[lane] = static_cast<std::uint8_t>(req.data[lane]);
    }
    write.mask = req.mask;
    write_bits = write;
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

ArbWriteAccum::ArbWriteAccum(std::string /*name*/, std::size_t bank, IMPL_CTOR)
    : bank_(bank) {
  UPDATE(updateReady)
      .reads(exwrite_val, dmaread_full_val, dmaread_val, write_rdy)
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
  exwrite_rdy      = bit(write_rdy == 1);
  dmaread_full_rdy = bit(exwrite_val == 0 && write_rdy == 1);
  dmaread_rdy      = bit(exwrite_val == 0 && dmaread_full_val == 0 &&
                         write_rdy == 1);
  zerowrite_rdy    = bit(exwrite_val == 0 && dmaread_full_val == 0 &&
                         dmaread_val == 0 && write_rdy == 1);
}

void ArbWriteAccum::updateWrite() {
  const bool exwrite      = exwrite_val      != 0;
  const bool dmaread_full = dmaread_full_val != 0;
  const bool dmaread      = dmaread_val      != 0;
  const bool zerowrite    = zerowrite_val    != 0;

  write_val = bit(exwrite || dmaread_full || dmaread || zerowrite);
  if (exwrite) {
    const auto req = *exwrite_bits;
    DmaReadResp write{};
    // address = bank * rows_per_bank + row_within_bank 
    write.laddr = makeAccAddr(static_cast<std::uint32_t>(bank_ * kAccBankRows) +
                              (req.addr & kAccBankRowMask),
                              /*do_accumulate=*/req.acc == 1);
    write.has_acc_bitwidth = true;
    // copy kDim elements from ExCtrl's request into byte-array format used by DmaReadResp
    // 1) visit each accumulator lane
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      const auto value = static_cast<std::uint32_t>(req.data[lane]);
      // 2) split each lane into four bytes and copy into the byte-array
      for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
        write.data[lane * sizeof(Acc) + byte] = static_cast<std::uint8_t>((value >> (8 * byte)) & 0xffu);
      }
    }
    // ExCtrl marks alignment units (i.e., which row elems/cols to write) per byte;
    // Accum's write port marks alignment units per element lanes.
    // Convert 4-byte mask bits into 1-lane mask bits by ORing the 4 bytes of each lane together.
    constexpr auto mask_bits_per_lane = sizeof(Acc) / kDefaultConfig.aligned_to; // 4B per accum lane / 1  alignment per byte
    write.mask = 0;
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      if ((static_cast<std::uint32_t>(req.mask) & (std::uint32_t{1} << (lane * mask_bits_per_lane))) != 0) {
        write.mask |= std::uint32_t{1} << lane;
      }
    }
    write_bits = write;
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
