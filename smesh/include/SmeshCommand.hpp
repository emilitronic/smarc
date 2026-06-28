// **********************************************************************
// smesh/include/SmeshCommand.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 27 2026
/*
Command encoding and helper functions for SmeshDevice::executeCustom().
*/
#pragma once

#include "SmeshLocalAddr.hpp"

#include <cstdint>

namespace smesh {

// Enumerates the available functions for the custom command interface.
enum class SmeshFunct : std::uint32_t {
  Config = 0,
  Mvin2 = 1,
  Mvin = 2,
  Mvout = 3,
  ComputeFlip = 4,
  ComputeStay = 5,
  Preload = 6,
  Flush = 7,
  Mvin3 = 14,
  StoreSpad = 23,
};

// Sub-kinds of configuration commands.
enum class ConfigKind : std::uint32_t {
  Execute = 0,
  Load = 1,
  Store = 2,
};

// Represents a local matrix in the SPAD.  This is used for passing matrix location and shape information in the rs1/rs2 fields of commands.
struct LocalMatrix {
  std::uint32_t row = 0;
  MatrixShape shape{};
};

constexpr std::uint32_t kLocalAddrBits = 32;
constexpr std::uint64_t kLocalAddrMask = 0xffffffffull;

// Note packed matrix operand form:
// bits [31:0]   local address / row
// bits [47:32]  cols
// bits [63:48]  rows
// Build encoded rs1/rs2 operanad
// Packs row (where matrix starts locally), cols (no. of cols), rows (no. of rows) into a 64-bit value for passing in rs2 = (rows << 48) | (cols << 32) | row (local addr)
inline std::uint64_t packLocal(std::uint32_t row, MatrixShape shape) {
  return (static_cast<std::uint64_t>(shape.rows) << (kLocalAddrBits + 16)) |
         (static_cast<std::uint64_t>(shape.cols) << kLocalAddrBits) |
         static_cast<std::uint64_t>(row);
}
// convenience overload allowing packLocal(makeAccAddr(8), shape) instead of packLocal(makeAccAddr(8).raw, shape)
// No need to unwrap type (and accidentally pass unencoded row number)
inline std::uint64_t packLocal(SmeshLocalAddr addr, MatrixShape shape) {
  return packLocal(addr.raw, shape);
}
// Decode encoded rs1/rs2 operand (inside SmeshDevice/SmeshShell)
inline LocalMatrix unpackLocal(std::uint64_t packed) {
  return LocalMatrix{
      static_cast<std::uint32_t>(packed & kLocalAddrMask),
      MatrixShape{
          static_cast<std::size_t>((packed >> (kLocalAddrBits + 16)) & 0xffffu),
          static_cast<std::size_t>((packed >> kLocalAddrBits) & 0xffffu),
      },
  };
}

constexpr std::uint32_t kConfigStateIdShift = 3;
constexpr std::uint32_t kConfigLoadBlockStrideShift = 16;
constexpr std::uint64_t kConfigLoadBlockStrideMask = 0xffffull;
constexpr std::uint32_t kConfigExecuteAStrideShift = 16;
constexpr std::uint32_t kConfigExecuteATransposeBit = 8;
constexpr std::uint32_t kConfigExecuteCStrideShift = 48;

inline std::uint64_t packConfig(ConfigKind kind,
                                std::uint32_t state_id = 0,
                                std::uint32_t ld_block_stride = 0) {
  return static_cast<std::uint64_t>(kind) |
         (static_cast<std::uint64_t>(state_id & 0x3u) << kConfigStateIdShift) |
         ((static_cast<std::uint64_t>(ld_block_stride) & kConfigLoadBlockStrideMask)
          << kConfigLoadBlockStrideShift);
}

inline std::uint32_t unpackConfigStateId(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigStateIdShift) & 0x3u);
}

inline std::uint32_t unpackConfigLoadBlockStride(std::uint64_t rs1) {
  return static_cast<std::uint32_t>(
      (rs1 >> kConfigLoadBlockStrideShift) & kConfigLoadBlockStrideMask);
}

inline std::uint64_t packConfigExecuteRs1(std::uint32_t a_stride,
                                         bool a_transpose = false) {
  return static_cast<std::uint64_t>(ConfigKind::Execute) |
         (static_cast<std::uint64_t>(a_stride & 0xffffu)
          << kConfigExecuteAStrideShift) |
         (static_cast<std::uint64_t>(a_transpose)
          << kConfigExecuteATransposeBit);
}

inline std::uint64_t packConfigExecuteRs2(std::uint32_t c_stride) {
  return static_cast<std::uint64_t>(c_stride & 0xffffu)
         << kConfigExecuteCStrideShift;
}

inline std::uint32_t unpackConfigExecuteAStride(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigExecuteAStrideShift) & 0xffffu);
}

inline bool unpackConfigExecuteATranspose(std::uint64_t rs1) {
  return ((rs1 >> kConfigExecuteATransposeBit) & 0x1u) != 0;
}

inline std::uint32_t unpackConfigExecuteCStride(std::uint64_t rs2) {
  return static_cast<std::uint32_t>((rs2 >> kConfigExecuteCStrideShift) & 0xffffu);
}

inline std::uint64_t packStoreSpadDestination(std::uint32_t local_addr,
                                              std::uint32_t stride = 1) {
  return (static_cast<std::uint64_t>(stride) << 32) | local_addr;
}
// STORE_SPAD convenience overload
inline std::uint64_t packStoreSpadDestination(SmeshLocalAddr local_addr,
                                              std::uint32_t stride = 1) {
  return packStoreSpadDestination(local_addr.raw, stride);
}

inline std::uint32_t unpackStoreSpadDestinationStride(std::uint64_t rs1) {
  return static_cast<std::uint32_t>(rs1 >> 32);
}

} // namespace smesh
