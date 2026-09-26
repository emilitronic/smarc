// **********************************************************************
// smesh/tests/reference/loop_matmul/LoopMatmulReference.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 25 2026

#pragma once

#include "SmeshCommand.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace smesh {
namespace tb {

struct PrimitiveCommand {
  SmeshFunct funct;
  std::uint64_t rs1;
  std::uint64_t rs2;
};

using LoopWsProgram = std::array<PrimitiveCommand, 6>;

// Preserve each generator's order; the top-level arbiter chooses their interleaving.
struct LoopWsCommands {
  std::vector<PrimitiveCommand> load_a;
  std::vector<PrimitiveCommand> load_b;
  std::vector<PrimitiveCommand> load_d;
  std::vector<PrimitiveCommand> preload;
  std::vector<PrimitiveCommand> compute;
  std::vector<PrimitiveCommand> store_c;
};

inline LoopWsCommands generateWsCommands(const LoopWsProgram& program) {
  constexpr std::array<SmeshFunct, 6> expected{
      SmeshFunct::LoopWsBounds, SmeshFunct::LoopWsAddrsAb,
      SmeshFunct::LoopWsAddrsDc, SmeshFunct::LoopWsStridesAb,
      SmeshFunct::LoopWsStridesDc, SmeshFunct::LoopWs};
  for (std::size_t i = 0; i < program.size(); ++i) {
    if (program[i].funct != expected[i]) {
      throw std::invalid_argument("expected the six ordinary LOOP_WS commands in order");
    }
  }

  const auto bounds = program[0].rs2;
  const auto pads = program[0].rs1;
  const auto i_tiles = bounds & 0xffffu;
  const auto j_tiles = (bounds >> 16) & 0xffffu;
  const auto k_tiles = (bounds >> 32) & 0xffffu;
  const auto b_spad_id = (program[5].rs1 >> 16) & 0x3u;
  const auto expanded_axes = (i_tiles > 1) + (j_tiles > 1) + (k_tiles > 1);
  if (i_tiles < 1 || i_tiles > 2 || j_tiles < 1 || j_tiles > 2 ||
      k_tiles < 1 || k_tiles > 2 || expanded_axes > 1 || pads != 0 ||
      (program[5].rs1 & ~(std::uint64_t{0x3} << 16)) != 0 ||
      program[5].rs2 != 0 || b_spad_id > 2) {
    throw std::invalid_argument("reference model supports one expanded axis, size 1..2, no padding or transpose");
  }
  if (j_tiles > kDefaultConfig.dma_max_bytes / (kDim * sizeof(Acc))) {
    throw std::invalid_argument("J exceeds one full-width DMA block");
  }

  // Loop 0 starts A and accumulator rows at zero. B's region end comes from
  // its LOOP_WS selector; selector zero uses the first half-SPAD boundary.
  const auto a_dram_addr = program[1].rs1;
  const auto b_dram_addr = program[1].rs2;
  const auto d_dram_addr = program[2].rs1;
  const auto c_dram_addr = program[2].rs2;
  if (a_dram_addr == 0 || b_dram_addr == 0 || d_dram_addr == 0 || c_dram_addr == 0) {
    throw std::invalid_argument("this case requires A, B, D, and C DRAM addresses");
  }
  const MatrixShape tile{kDim, kDim};
  const MatrixShape a_block{kDim, static_cast<std::size_t>(k_tiles) * kDim};
  const MatrixShape b_block{kDim, static_cast<std::size_t>(j_tiles) * kDim};
  const auto half_spad = static_cast<std::uint32_t>(kSpRows / 2);
  const auto b_end = b_spad_id == 0 ? half_spad :
                     static_cast<std::uint32_t>(b_spad_id) * half_spad;
  const auto b_start = b_end - static_cast<std::uint32_t>(k_tiles * j_tiles * kDim);
  const auto garbage = packLocal(std::uint32_t{0xffffffff}, tile);

  LoopWsCommands out;
  for (std::uint32_t k = 0; k < k_tiles; ++k) {
    const auto b_row = b_start + k * static_cast<std::uint32_t>(j_tiles * kDim);
    const auto b_offset = (static_cast<std::uint64_t>(k) * program[3].rs2 * kDim * sizeof(Elem)) & 0xffffffffull;
    out.load_b.push_back({SmeshFunct::Mvin2, b_dram_addr + b_offset,
                          packLocal(makeSpAddr(b_row), b_block)});
  }
  for (std::uint32_t i = 0; i < i_tiles; ++i) {
    const auto a_row = i * static_cast<std::uint32_t>(k_tiles * kDim);
    const auto c_row = i * static_cast<std::uint32_t>(j_tiles * kDim);
    // Gemmini masks each DRAM tile offset to 32 bits before adding its base.
    const auto a_offset = (static_cast<std::uint64_t>(i) * program[3].rs1 * kDim * sizeof(Elem)) & 0xffffffffull;
    const auto d_offset = (static_cast<std::uint64_t>(i) * program[4].rs1 * kDim * sizeof(Acc)) & 0xffffffffull;
    const auto c_offset = (static_cast<std::uint64_t>(i) * program[4].rs2 * kDim * sizeof(Elem)) & 0xffffffffull;

    out.load_a.push_back({SmeshFunct::Mvin, a_dram_addr + a_offset,
                          packLocal(makeSpAddr(a_row), a_block)});
    out.load_d.push_back({SmeshFunct::Mvin3, d_dram_addr + d_offset,
                          packLocal(makeAccAddr(c_row), b_block)});
    out.store_c.push_back({SmeshFunct::Mvout, c_dram_addr + c_offset,
                           packLocal(makeAccAddr(c_row), b_block)});
  }
  // Execute advances K, then J, then I. Weights reload at each K step.
  for (std::uint32_t k = 0; k < k_tiles; ++k) {
    for (std::uint32_t j = 0; j < j_tiles; ++j) {
      for (std::uint32_t i = 0; i < i_tiles; ++i) {
        const auto a_row = (i * k_tiles + k) * static_cast<std::uint32_t>(kDim);
        const auto b_row = b_start + (k * j_tiles + j) * static_cast<std::uint32_t>(kDim);
        const auto c_row = (i * j_tiles + j) * static_cast<std::uint32_t>(kDim);
        out.preload.push_back({SmeshFunct::Preload,
                               i == 0 ? packLocal(makeSpAddr(b_row), tile) : garbage,
                               packLocal(makeAccAddr(c_row, k != 0), tile)});
        out.compute.push_back({i == 0 ? SmeshFunct::ComputeFlip : SmeshFunct::ComputeStay,
                               packLocal(makeSpAddr(a_row), tile), garbage});
      }
    }
  }
  return out;
}

} // namespace tb
} // namespace smesh
