// **********************************************************************
// smesh/tests/reference/loop_matmul/tb_loop_matmul_reference.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 25 2026

#pragma once

#include "SmeshCommand.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace smesh {
namespace tb {

struct PrimitiveCommand {
  SmeshFunct funct;
  std::uint64_t rs1;
  std::uint64_t rs2;
};

using LoopWsProgram = std::array<PrimitiveCommand, 6>;

// Each field is one generator's command; their interleaving depends on arbitration.
struct LoopWsSingleTileCommands {
  PrimitiveCommand load_a;
  PrimitiveCommand load_b;
  PrimitiveCommand load_d;
  PrimitiveCommand preload;
  PrimitiveCommand compute;
  PrimitiveCommand store_c;
};

inline LoopWsSingleTileCommands generateSingleTileWs(const LoopWsProgram& program) {
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
  if (i_tiles != 1 || j_tiles != 1 || k_tiles != 1 || pads != 0 ||
      program[5].rs1 != 0 || program[5].rs2 != 0) {
    throw std::invalid_argument("reference model currently supports one unpadded WS tile");
  }

  // On reset, loop 0 starts A and accumulator rows at zero; B ends halfway
  // through SPAD. Strides are configured by commands 3 and 4 but do not affect
  // the first and only tile.
  const auto a_dram_addr = program[1].rs1;
  const auto b_dram_addr = program[1].rs2;
  const auto d_dram_addr = program[2].rs1;
  const auto c_dram_addr = program[2].rs2;
  if (a_dram_addr == 0 || b_dram_addr == 0 || d_dram_addr == 0 || c_dram_addr == 0) {
    throw std::invalid_argument("this case requires A, B, D, and C DRAM addresses");
  }
  const MatrixShape tile{kDim, kDim};
  const auto b_start = static_cast<std::uint32_t>(kSpRows / 2 - kDim);

  return {
      {SmeshFunct::Mvin, a_dram_addr, packLocal(makeSpAddr(0), tile)},
      {SmeshFunct::Mvin2, b_dram_addr, packLocal(makeSpAddr(b_start), tile)},
      {SmeshFunct::Mvin3, d_dram_addr, packLocal(makeAccAddr(0), tile)},
      {SmeshFunct::Preload, packLocal(makeSpAddr(b_start), tile),
       packLocal(makeAccAddr(0), tile)},
      {SmeshFunct::ComputeFlip, packLocal(makeSpAddr(0), tile),
       packLocal(std::uint32_t{0xffffffff}, tile)},
      {SmeshFunct::Mvout, c_dram_addr, packLocal(makeAccAddr(0), tile)},
  };
}

} // namespace tb
} // namespace smesh
