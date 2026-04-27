// **********************************************************************
// smesh/include/SmeshCommand.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 27 2026
/*
Command encoding and helper functions for SmeshDevice::executeCustom().
*/
#pragma once

#include "SmeshTypes.hpp"

#include <cstdint>

namespace smesh {

// Enumerates the available functions for the custom command interface.
enum class SmeshFunct : std::uint32_t {
  Config = 0,
  Mvin = 2,
  Mvout = 3,
  ComputePreloaded = 4,
  ComputeAccumulated = 5,
  Preload = 6,
  Flush = 7,
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

// Packs row (where matrix starts locally), cols (no. of cols), rows (no. of rows) into a 64-bit value for passing in rs2 = (rows << 48) | (cols << 32) | row (local addr)
inline std::uint64_t packLocal(std::uint32_t row, MatrixShape shape) {
  return (static_cast<std::uint64_t>(shape.rows) << (kLocalAddrBits + 16)) |
         (static_cast<std::uint64_t>(shape.cols) << kLocalAddrBits) |
         static_cast<std::uint64_t>(row);
}
inline LocalMatrix unpackLocal(std::uint64_t packed) {
  return LocalMatrix{
      static_cast<std::uint32_t>(packed & kLocalAddrMask),
      MatrixShape{
          static_cast<std::size_t>((packed >> (kLocalAddrBits + 16)) & 0xffffu),
          static_cast<std::size_t>((packed >> kLocalAddrBits) & 0xffffu),
      },
  };
}

inline std::uint64_t packConfig(ConfigKind kind) {
  return static_cast<std::uint64_t>(kind);
}

} // namespace smesh
