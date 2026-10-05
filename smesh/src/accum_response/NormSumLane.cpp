// **********************************************************************
// smesh/src/accum_response/NormSumLane.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormSumLane.hpp"

#include <cassert>
#include <cstdint>

namespace smesh {

NormSumLane::NormSumLane(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(chunk_val, chunk_bits).writes(result_val, result_bits);
}

void NormSumLane::update() {
  result_val = chunk_val;
  NormSumResult result{};
  if (chunk_val == 1) { // valid chunk is present
    const auto chunk  = *chunk_bits;
    const auto len    = static_cast<std::size_t>(static_cast<std::uint16_t>(chunk.len)); // number of valid lanes in the chunk
    assert(len <= kDim);
    result.slot       = chunk.slot;
    std::uint32_t sum = 0;
    const auto cmd    = static_cast<NormCmd>(static_cast<std::uint8_t>(chunk.cmd));
    for (std::size_t lane = 0; lane < len; ++lane) {
      if (cmd == NormCmd::Variance || cmd == NormCmd::InvStddev) {
        const auto difference = static_cast<std::int64_t>(chunk.data[lane]) - chunk.mean;
        const auto magnitude  = static_cast<std::uint64_t>(difference < 0 ? -difference : difference);
        sum += static_cast<std::uint32_t>(magnitude * magnitude);
      } else {
        sum += static_cast<std::uint32_t>(chunk.data[lane]);
      }
    }
    result.sum = u32(sum);
  }
  result_bits = result;
}

void NormSumLane::reset() {
  result_val.reset(0);
  result_bits.reset(NormSumResult{});
}

} // namespace smesh
