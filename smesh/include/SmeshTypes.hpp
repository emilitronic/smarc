// **********************************************************************
// smesh/include/SmeshTypes.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 26 2026
/*
Common types and constants for the Smesh project.
*/
#pragma once

#include "SmeshConfig.hpp"

#include <cstddef>
#include <cstdint>

namespace smesh {

constexpr std::size_t kDim = kDefaultConfig.dim;                             // cols per row
constexpr std::size_t kSpBanks = kDefaultConfig.sp_banks;
constexpr std::size_t kSpBankRows = kDefaultConfig.sp_bank_rows;
constexpr std::size_t kSpRows = kSpBanks * kSpBankRows;                      // total rows in SP
constexpr std::size_t kAccBanks = kDefaultConfig.acc_banks;
constexpr std::size_t kAccBankRows = kDefaultConfig.acc_bank_rows;
constexpr std::size_t kAccRows = kAccBanks * kAccBankRows;                   // total rows in accumulator
constexpr std::size_t kLoadStates = kDefaultConfig.load_states;              // mvin/mvin2/mvin3 stride states
constexpr std::size_t kRsLoadEntries = kDefaultConfig.rs_load_entries;       // M4v0 RS load slots
constexpr std::size_t kRsExecuteEntries = kDefaultConfig.rs_execute_entries; // M4v0 RS execute slots
constexpr std::size_t kRsStoreEntries = kDefaultConfig.rs_store_entries;     // M4v0 RS store slots

static_assert(kSpBanks > 0 && (kSpBanks & (kSpBanks - 1)) == 0,
              "scratchpad bank count must be a power of two");
static_assert(kSpBankRows > 0 && (kSpBankRows & (kSpBankRows - 1)) == 0,
              "scratchpad rows per bank must be a power of two");
static_assert(kAccBanks > 0 && (kAccBanks & (kAccBanks - 1)) == 0,
              "accumulator bank count must be a power of two");
static_assert(kAccBankRows > 0 && (kAccBankRows & (kAccBankRows - 1)) == 0,
              "accumulator rows per bank must be a power of two");

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

// compile-time addr widths from memory geometry
constexpr std::size_t kSpAddrBits = log2Up(kSpRows);
constexpr std::size_t kAccAddrBits = log2Up(kAccRows);
constexpr std::size_t kLocalAddrDataBits = kSpAddrBits > kAccAddrBits ? kSpAddrBits : kAccAddrBits;
constexpr std::size_t kSpBankBits = log2Up(kSpBanks);
constexpr std::size_t kSpBankRowBits = log2Up(kSpBankRows);
constexpr std::size_t kAccBankBits = log2Up(kAccBanks);
constexpr std::size_t kAccBankRowBits = log2Up(kAccBankRows);
// masks to extract location from low-bit fields (high-bit fields are metadata)
constexpr std::uint32_t kLocalAddrDataMask = lowBitMask(kLocalAddrDataBits);
constexpr std::uint32_t kSpAddrMask = lowBitMask(kSpAddrBits);
constexpr std::uint32_t kAccAddrMask = lowBitMask(kAccAddrBits);
constexpr std::uint32_t kSpBankRowMask = lowBitMask(kSpBankRowBits);
constexpr std::uint32_t kAccBankRowMask = lowBitMask(kAccBankRowBits);
// locate metadata in addr fields
constexpr std::uint32_t kLocalAddrGarbageMask = std::uint32_t{1} << kLocalAddrDataBits;
constexpr std::uint32_t kLocalAddrNormShift = 26;
constexpr std::uint32_t kLocalAddrReadFullAccRowMask = std::uint32_t{1} << 29;
constexpr std::uint32_t kLocalAddrAccumulateMask = std::uint32_t{1} << 30;
constexpr std::uint32_t kLocalAddrIsAccMask = std::uint32_t{1} << 31;

static_assert(kLocalAddrDataBits + 6 < 32, "local-address data and metadata must fit in 32 bits");

using Elem = std::int8_t;
using Acc = std::int32_t;

struct MatrixShape {
  std::size_t rows = 0;
  std::size_t cols = 0;
};

struct SmeshLocalAddr {
  std::uint32_t raw = 0;
  // accessor functions to decode our raw addr field value
  constexpr std::uint32_t data() const {
    return raw & kLocalAddrDataMask; // low physical addr bits
  }
  constexpr bool is_acc_addr() const {
    return (raw & kLocalAddrIsAccMask) != 0; // spad or accu?
  }
  constexpr bool accumulate() const {
    return (raw & kLocalAddrAccumulateMask) != 0; // overwrite or accumulate in accu?
  }
  constexpr bool read_full_acc_row() const {
    return (raw & kLocalAddrReadFullAccRowMask) != 0; // full-width accu read
  }
  constexpr std::uint32_t norm_cmd() const {
    return (raw >> kLocalAddrNormShift) & 0x7u; // three-bit normalization operation
  }
  constexpr bool is_garbage() const {
    return (raw & kLocalAddrGarbageMask) != 0; // invalid/padding addr
  }
  constexpr std::uint32_t sp_bank() const {
    return kSpBankBits == 0
               ? 0u
               : (data() & kSpAddrMask) >> kSpBankRowBits; // return sp bank number
  }
  constexpr std::uint32_t sp_row() const {
    return data() & kSpBankRowMask; // sp row in given bank
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
    return data() & kSpAddrMask; // flattened spad row address (bank*rows-per-bank + row-in-bank)
  }
  constexpr std::uint32_t full_acc_addr() const {
    return data() & kAccAddrMask; //flattened accu row address
  }
};

struct SmeshLocalAddrAddResult {
  SmeshLocalAddr addr{}; // wrapped/truncated resulting addr
  bool overflow = false; // whether addition crossed mem boundary
};

constexpr SmeshLocalAddr makeLocalAddr(std::uint32_t raw) {
  return SmeshLocalAddr{raw}; // just store bits in SmeshLocalAddr type
}

constexpr SmeshLocalAddr makeSpAddr(std::uint32_t full_sp_addr) {
  return SmeshLocalAddr{full_sp_addr & kSpAddrMask}; // make encoded spad addr, from flat row number (metadata set to 0)
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
      ((norm_cmd & 0x7u) << kLocalAddrNormShift)}; // make encoded accum addr (more complex init metadata)
}

// calculate address and overflow together, wraps at selected local mem size, preserves metadata
constexpr SmeshLocalAddrAddResult add_with_overflow(SmeshLocalAddr addr,
                                                     std::uint32_t offset) {
  const std::uint64_t sum = static_cast<std::uint64_t>(addr.data()) + offset;
  const std::size_t overflow_bit = addr.is_acc_addr() ? kAccAddrBits : kSpAddrBits;
  return SmeshLocalAddrAddResult{
      SmeshLocalAddr{
          (addr.raw & ~kLocalAddrDataMask) |
          (static_cast<std::uint32_t>(sum) & kLocalAddrDataMask)},
      ((sum >> overflow_bit) & 0x1u) != 0};
}
// what "+" means for SmeshLocalAddr (changes addr-data bits, preserves metadata, wraps at local-memory capacity)
constexpr SmeshLocalAddr operator+(SmeshLocalAddr addr, std::uint32_t offset) {
  return add_with_overflow(addr, offset).addr; // resulting addr, discards overflow flag
}

} // namespace smesh
