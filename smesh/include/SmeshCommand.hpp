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
  Config      =  0,
  Mvin2       =  1,
  Mvin        =  2,
  Mvout       =  3,
  ComputeFlip =  4,
  ComputeStay =  5,
  Preload     =  6,
  Flush       =  7,
  Mvin3       = 14,
  StoreSpad   = 23,
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

// bit encoding of CONFIG commands
constexpr std::uint32_t kConfigStateIdShift             =  3;
constexpr std::uint32_t kConfigLoadBlockStrideShift     = 16;
constexpr std::uint64_t kConfigLoadBlockStrideMask      = 0xffffull;
constexpr std::uint32_t kConfigExecuteDataflowBit       =  2;
constexpr std::uint32_t kConfigExecuteActivationShift   =  3;
constexpr std::uint32_t kConfigExecuteSetOnlyStridesBit =  7;
constexpr std::uint32_t kConfigExecuteATransposeBit     =  8;
constexpr std::uint32_t kConfigExecuteBTransposeBit     =  9;
constexpr std::uint32_t kConfigExecuteAStrideShift      = 16;
constexpr std::uint32_t kConfigExecuteAccScaleShift     = 32;
constexpr std::uint32_t kConfigExecuteCStrideShift      =  0;
constexpr std::uint32_t kConfigExecuteRelu6ShiftShift   = 16;
constexpr std::uint32_t kConfigExecuteInShiftShift      = 32;
// Packs rs1 for generic CONFIG commands; CONFIG_EX uses packConfigExecuteRs1/rs2
inline std::uint64_t packConfig(ConfigKind kind, std::uint32_t state_id = 0, std::uint32_t ld_block_stride = 0) {
  return static_cast<std::uint64_t>(kind) |
         (static_cast<std::uint64_t>(state_id & 0x3u) << kConfigStateIdShift) |
         ((static_cast<std::uint64_t>(ld_block_stride) & kConfigLoadBlockStrideMask)
          << kConfigLoadBlockStrideShift);
}
// Extracts the generic CONFIG state selector from rs1
inline std::uint32_t unpackConfigStateId(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigStateIdShift) & 0x3u);
}
// Extracts the generic CONFIG load block stride from rs1
inline std::uint32_t unpackConfigLoadBlockStride(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigLoadBlockStrideShift) & kConfigLoadBlockStrideMask);
}

inline std::uint64_t packConfigExecuteRs1(std::uint32_t a_stride,
                                          bool a_transpose         = false,
                                          bool b_transpose         = false,
                                          std::uint32_t dataflow   = 0,
                                          bool set_only_strides    = false,
                                          std::uint32_t activation = 0,
                                          std::uint32_t acc_scale  = 0) {
  return static_cast<std::uint64_t>(ConfigKind::Execute) |
         (static_cast<std::uint64_t>(dataflow & 0x1u) << kConfigExecuteDataflowBit) |
         (static_cast<std::uint64_t>(activation & 0x3u) << kConfigExecuteActivationShift) |
         (static_cast<std::uint64_t>(set_only_strides) << kConfigExecuteSetOnlyStridesBit) |
         (static_cast<std::uint64_t>(a_transpose) << kConfigExecuteATransposeBit) |
         (static_cast<std::uint64_t>(b_transpose) << kConfigExecuteBTransposeBit) |
         (static_cast<std::uint64_t>(a_stride & 0xffffu) << kConfigExecuteAStrideShift) |
         (static_cast<std::uint64_t>(acc_scale) << kConfigExecuteAccScaleShift);
}

inline std::uint64_t packConfigExecuteRs2(std::uint32_t c_stride,
                                          std::uint32_t in_shift    = 0,
                                          std::uint32_t relu6_shift = 0) {
  return (static_cast<std::uint64_t>(c_stride & 0xffffu) << kConfigExecuteCStrideShift) |
         (static_cast<std::uint64_t>(relu6_shift & 0xffffu) << kConfigExecuteRelu6ShiftShift) |
         (static_cast<std::uint64_t>(in_shift) << kConfigExecuteInShiftShift);
}
// Extracts CONFIG_EX rs1[2], the execute dataflow selector
inline std::uint32_t unpackConfigExecuteDataflow(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigExecuteDataflowBit) & 0x1u);
}
// Extracts CONFIG_EX rs1[4:3], the activation selector
inline std::uint32_t unpackConfigExecuteActivation(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigExecuteActivationShift) & 0x3u);
}
//Extracts CONFIG_EX rs1[7], which limits CONFIG_EX to stride updates
inline bool unpackConfigExecuteSetOnlyStrides(std::uint64_t rs1) {
  return ((rs1 >> kConfigExecuteSetOnlyStridesBit) & 0x1u) != 0;
}
// Extracts CONFIG_EX rs1[31:16], the A-address stride
inline std::uint32_t unpackConfigExecuteAStride(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigExecuteAStrideShift) & 0xffffu);
}
// Extracts CONFIG_EX rs1[8], the A transpose flag
inline bool unpackConfigExecuteATranspose(std::uint64_t rs1) {
  return ((rs1 >> kConfigExecuteATransposeBit) & 0x1u) != 0;
}
// Extracts CONFIG_EX rs1[9], the B/D transpose flag
inline bool unpackConfigExecuteBTranspose(std::uint64_t rs1) {
  return ((rs1 >> kConfigExecuteBTransposeBit) & 0x1u) != 0;
}
// Extracts CONFIG_EX rs1[63:32], the accumulator scale field
inline std::uint32_t unpackConfigExecuteAccScale(std::uint64_t rs1) {
  return static_cast<std::uint32_t>((rs1 >> kConfigExecuteAccScaleShift) & 0xffffffffull);
}
// Extracts CONFIG_EX rs2[15:0], the C-address stride
inline std::uint32_t unpackConfigExecuteCStride(std::uint64_t rs2) {
  return static_cast<std::uint32_t>((rs2 >> kConfigExecuteCStrideShift) & 0xffffu);
}
// Extracts CONFIG_EX rs2[31:16], the ReLU6 shift field
inline std::uint32_t unpackConfigExecuteRelu6Shift(std::uint64_t rs2) {
  return static_cast<std::uint32_t>((rs2 >> kConfigExecuteRelu6ShiftShift) & 0xffffu);
}
// Extracts CONFIG_EX rs2[63:32], the mesh input shift amount
inline std::uint32_t unpackConfigExecuteInShift(std::uint64_t rs2) {
  return static_cast<std::uint32_t>((rs2 >> kConfigExecuteInShiftShift) & 0xffffffffull);
}
// Packs STORE_SPAD destination metadata: local address plus stride
inline std::uint64_t packStoreSpadDestination(std::uint32_t local_addr, std::uint32_t stride = 1) {
  return (static_cast<std::uint64_t>(stride) << 32) | local_addr;
}
// STORE_SPAD convenience overload
inline std::uint64_t packStoreSpadDestination(SmeshLocalAddr local_addr, std::uint32_t stride = 1) {
  return packStoreSpadDestination(local_addr.raw, stride);
}
// Extracts the STORE_SPAD destination stride
inline std::uint32_t unpackStoreSpadDestinationStride(std::uint64_t rs1) {
  return static_cast<std::uint32_t>(rs1 >> 32);
}

} // namespace smesh
