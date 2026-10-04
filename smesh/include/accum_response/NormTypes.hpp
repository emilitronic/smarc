// **********************************************************************
// smesh/include/accum_response/NormTypes.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#pragma once

#include "NormalizerState.hpp"

#include <array>
#include <limits>

namespace smesh {

struct NormSavedPackets {
  std::array<AccNormReq, kNormStatsSlots> packet{};
};

struct NormStatsRegs {
  std::array<u16, kNormStatsSlots> elems_left{};
  std::array<u16, kNormStatsSlots> count{};
  std::array<u32, kNormStatsSlots> sum{};
  std::array<Acc, kNormStatsSlots> running_max{
      std::numeric_limits<Acc>::min(), std::numeric_limits<Acc>::min()};
  std::array<Acc, kNormStatsSlots> max{
      std::numeric_limits<Acc>::min(), std::numeric_limits<Acc>::min()};
};

struct NormChunk {
  MeshAccumRow data{};
  u16 len  = 0;
  u8  slot = 0;
  bit last = 0;
};

struct NormSumResult {
  u8  slot = 0;
  u32 sum  = 0;
};

struct NormMaxResult {
  u8 slot = 0;
  Acc max = std::numeric_limits<Acc>::min();
};

} // namespace smesh
