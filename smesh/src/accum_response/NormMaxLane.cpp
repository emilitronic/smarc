// **********************************************************************
// smesh/src/accum_response/NormMaxLane.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormMaxLane.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

namespace smesh {

NormMaxLane::NormMaxLane(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(chunk_val, chunk_bits).writes(result_val, result_bits);
}

void NormMaxLane::update() {
  result_val = chunk_val;
  NormMaxResult result{};
  if (chunk_val == 1) {
    const auto chunk = *chunk_bits;
    const auto len = static_cast<std::size_t>(static_cast<std::uint16_t>(chunk.len));
    assert(len > 0 && len <= kDim);
    result.slot = chunk.slot;
    for (std::size_t lane = 0; lane < len; ++lane) {
      result.max = std::max(result.max, chunk.data[lane]);
    }
  }
  result_bits = result;
}

void NormMaxLane::reset() {
  result_val.reset(0);
  result_bits.reset(NormMaxResult{});
}

} // namespace smesh
