// **********************************************************************
// smesh/include/SmeshLocalAddr.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 28 2026
/*
Encoded local-memory addresses for smesh and support functions for dealing with it.
*/
#pragma once

#include "SmeshTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace smesh {

constexpr std::size_t log2Up(std::size_t value) {
  std::size_t bits = 0;
  for (std::size_t max_index = value - 1; max_index != 0; max_index >>= 1) {
    ++bits;
  }
  return bits;
}

constexpr std::uint32_t lowBitMask(std::size_t bits) {
  return bits == 0 ? 0u : (std::uint32_t{1} << bits) - 1u;
}

// Compile-time address widths from memory geometry.
constexpr std::size_t kSpAddrBits = log2Up(kSpRows);
constexpr std::size_t kAccAddrBits = log2Up(kAccRows);
constexpr std::size_t kLocalAddrDataBits = kSpAddrBits > kAccAddrBits ? kSpAddrBits : kAccAddrBits;
constexpr std::size_t kSpBankBits = log2Up(kSpBanks);
constexpr std::size_t kSpBankRowBits = log2Up(kSpBankRows);
constexpr std::size_t kAccBankBits = log2Up(kAccBanks);
constexpr std::size_t kAccBankRowBits = log2Up(kAccBankRows);

// Masks to extract location from low-bit fields; high-bit fields are metadata.
constexpr std::uint32_t kLocalAddrDataMask = lowBitMask(kLocalAddrDataBits);
constexpr std::uint32_t kSpAddrMask = lowBitMask(kSpAddrBits);
constexpr std::uint32_t kAccAddrMask = lowBitMask(kAccAddrBits);
constexpr std::uint32_t kSpBankRowMask = lowBitMask(kSpBankRowBits);
constexpr std::uint32_t kAccBankRowMask = lowBitMask(kAccBankRowBits);

// Metadata locations in the encoded address.
constexpr std::uint32_t kLocalAddrGarbageMask = std::uint32_t{1} << kLocalAddrDataBits;
constexpr std::uint32_t kLocalAddrNormShift = 26;
constexpr std::uint32_t kLocalAddrReadFullAccRowMask = std::uint32_t{1} << 29;
constexpr std::uint32_t kLocalAddrAccumulateMask = std::uint32_t{1} << 30;
constexpr std::uint32_t kLocalAddrIsAccMask = std::uint32_t{1} << 31;

static_assert(kLocalAddrDataBits + 6 < 32, "local-address data and metadata must fit in 32 bits");

struct SmeshLocalAddr {
  std::uint32_t raw = 0;

  // Accessor functions to decode our raw address-field value.
  constexpr std::uint32_t data() const {
    return raw & kLocalAddrDataMask; // low physical address bits
  }
  constexpr bool is_acc_addr() const {
    return (raw & kLocalAddrIsAccMask) != 0; // scratchpad or accumulator?
  }
  constexpr bool accumulate() const {
    return (raw & kLocalAddrAccumulateMask) != 0; // overwrite or accumulate?
  }
  constexpr bool read_full_acc_row() const {
    return (raw & kLocalAddrReadFullAccRowMask) != 0; // full-width acc read
  }
  constexpr std::uint32_t norm_cmd() const {
    return (raw >> kLocalAddrNormShift) & 0x7u; // three-bit normalization op
  }
  constexpr bool is_garbage() const {
    return (raw & kLocalAddrGarbageMask) != 0; // invalid/padding address
  }
  constexpr std::uint32_t sp_bank() const {
    return kSpBankBits == 0
               ? 0u
               : (data() & kSpAddrMask) >> kSpBankRowBits; // SP bank number
  }
  constexpr std::uint32_t sp_row() const {
    return data() & kSpBankRowMask; // row within the selected SP bank
  }
  constexpr std::uint32_t acc_bank() const {
    return kAccBankBits == 0
               ? 0u
               : (data() & kAccAddrMask) >> kAccBankRowBits;
  }
  constexpr std::uint32_t acc_row() const {
    return data() & kAccBankRowMask;
  }
  constexpr std::uint32_t full_sp_addr() const {
    return data() & kSpAddrMask; // flattened SP row address
  }
  constexpr std::uint32_t full_acc_addr() const {
    return data() & kAccAddrMask; // flattened accumulator row address
  }
};

struct SmeshLocalAddrAddResult {
  SmeshLocalAddr addr{}; // wrapped/truncated resulting address
  bool overflow = false; // whether addition crossed the memory boundary
};

constexpr SmeshLocalAddr makeLocalAddr(std::uint32_t raw) {
  return SmeshLocalAddr{raw}; // store existing encoded bits in the address type
}

constexpr SmeshLocalAddr makeSpAddr(std::uint32_t full_sp_addr) {
  // Make encoded SP address from a flat row number; metadata remains zero.
  return SmeshLocalAddr{full_sp_addr & kSpAddrMask};
}

constexpr SmeshLocalAddr makeAccAddr(std::uint32_t full_acc_addr,
                                     bool do_accumulate = false,
                                     bool read_full = false,
                                     std::uint32_t norm_cmd = 0) {
  return SmeshLocalAddr{
      (full_acc_addr & kAccAddrMask) |
      kLocalAddrIsAccMask |
      (do_accumulate ? kLocalAddrAccumulateMask : 0u) |
      (read_full ? kLocalAddrReadFullAccRowMask : 0u) |
      ((norm_cmd & 0x7u) << kLocalAddrNormShift)}; // encoded accumulator address
}

// Calculate address and overflow together, wrapping at the selected local
// memory size while preserving metadata.
constexpr SmeshLocalAddrAddResult add_with_overflow(SmeshLocalAddr addr,
                                                     std::uint32_t offset) {
  const std::uint64_t sum = static_cast<std::uint64_t>(addr.data()) + offset;
  const std::size_t overflow_bit =
      addr.is_acc_addr() ? kAccAddrBits : kSpAddrBits;
  return SmeshLocalAddrAddResult{
      SmeshLocalAddr{
          (addr.raw & ~kLocalAddrDataMask) |
          (static_cast<std::uint32_t>(sum) & kLocalAddrDataMask)},
      ((sum >> overflow_bit) & 0x1u) != 0};
}

constexpr SmeshLocalAddr operator+(SmeshLocalAddr addr, std::uint32_t offset) {
  // Return the resulting address and intentionally discard the overflow flag.
  return add_with_overflow(addr, offset).addr;
}

} // namespace smesh
