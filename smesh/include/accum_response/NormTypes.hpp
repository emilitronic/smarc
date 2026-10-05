// **********************************************************************
// smesh/include/accum_response/NormTypes.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#pragma once

#include "NormalizerState.hpp"

#include <array>
#include <limits>

namespace smesh {

// Holds the accepted input packet for each stats slot.
struct NormSavedPackets {
  std::array<AccNormReq, kNormStatsSlots> packet{};
};

// Holds the running and completed statistics for each slot.
struct NormStatsRegs {
  std::array<u16, kNormStatsSlots> elems_left{};
  std::array<u16, kNormStatsSlots> count{};
  std::array<u32, kNormStatsSlots> sum{};
  std::array<Acc, kNormStatsSlots> mean{};
  std::array<Acc, kNormStatsSlots> variance{};
  std::array<Acc, kNormStatsSlots> stddev{};
  std::array<u32, kNormStatsSlots> inv_stddev{};
  std::array<u32, kNormStatsSlots> inv_sum_exp{};
  std::array<Acc, kNormStatsSlots> running_max{std::numeric_limits<Acc>::min(), std::numeric_limits<Acc>::min()};
  std::array<Acc, kNormStatsSlots> max{std::numeric_limits<Acc>::min(), std::numeric_limits<Acc>::min()};
};

// Carries one selected row chunk from NormRowChunk to a reduction lane.
struct NormChunk {
  MeshAccumRow data{};   // selected row elements
  u16          len  = 0; // how many valid elements in chunk
  u8           slot = 0; // which stat slot elements belong to
  u8           cmd  = 0; // which op lane should perform; norm cmd attached to packet for that stats slot (Sum, Mean, Variance, Max, Reset, etc.)
  Acc          mean = 0; // for Variance, the selected slot's mean is carried with the chunk
  Acc          max = 0;
  u32          igelu_qb = 0;
  u32          igelu_qc = 0;
  bit          last = 0; // whether thisis the final chunk
};

// Carries a sum-lane result and its stats slot.
struct NormSumResult {
  u8  slot = 0;
  u32 sum  = 0;
};

// Carries a max-lane result and its stats slot.
struct NormMaxResult {
  u8  slot = 0;
  Acc max  = std::numeric_limits<Acc>::min();
};

// Holds a mean-divider result until its output cycle.
struct NormMeanPending {
  bit valid = 0;
  u8  slot  = 0;
  Acc value = 0;
};

// Holds a square-root result until its output cycle.
struct NormSqrtPending {
  bit valid = 0;
  u8  slot  = 0;
  Acc value = 0;
};

// Holds a reciprocal result until its output cycle.
struct NormReciprocalPending {
  bit valid = 0;
  u8  slot  = 0;
  u32 value = 0;
};

// Holds a scaled reciprocal result until its output cycle.
struct NormScalePending {
  bit valid = 0;
  u8  slot  = 0;
  u32 value = 0;
};

} // namespace smesh
