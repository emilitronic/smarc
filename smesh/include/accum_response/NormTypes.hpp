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
  std::array<Acc, kNormStatsSlots> mean{};
  std::array<Acc, kNormStatsSlots> variance{};
  std::array<Acc, kNormStatsSlots> running_max{std::numeric_limits<Acc>::min(), std::numeric_limits<Acc>::min()};
  std::array<Acc, kNormStatsSlots> max{std::numeric_limits<Acc>::min(), std::numeric_limits<Acc>::min()};
};

// bundle of fields carrying selected piece of row from NormRowChunk to NormSumLane or NormMaxLane
struct NormChunk {
  MeshAccumRow data{};   // selected row elements
  u16          len  = 0; // how many valid elements in chunk
  u8           slot = 0; // which stat slot elements belong to
  u8           cmd  = 0; // which op lane should perform; norm cmd attached to packet for that stats slot (Sum, Mean, Variance, Max, Reset, etc.)
  Acc          mean = 0; // for Variance, the selected slot's mean is carried with the chunk
  bit          last = 0; // whether thisis the final chunk
};

// output of NormSumLane
struct NormSumResult {
  u8  slot = 0;
  u32 sum  = 0;
};

struct NormMaxResult {
  u8  slot = 0;
  Acc max  = std::numeric_limits<Acc>::min();
};

struct NormMeanPending {
  bit valid = 0;
  u8  slot  = 0;
  Acc value = 0;
};

} // namespace smesh
