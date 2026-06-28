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

using Elem = std::int8_t;
using Acc = std::int32_t;

struct MatrixShape {
  std::size_t rows = 0;
  std::size_t cols = 0;
};

struct SmeshLocalAddr {
  std::uint32_t raw = 0;
};
// local address helpers
inline SmeshLocalAddr makeLocalAddr(std::uint32_t raw) {
  return SmeshLocalAddr{raw};
}
inline SmeshLocalAddr addLocalAddr(SmeshLocalAddr addr, std::uint32_t rows_touched) {
  return SmeshLocalAddr{addr.raw + rows_touched};
}
inline bool addLocalAddrOverflows(SmeshLocalAddr addr, std::uint32_t rows_touched) {
  return addLocalAddr(addr, rows_touched).raw < addr.raw;
}

} // namespace smesh
