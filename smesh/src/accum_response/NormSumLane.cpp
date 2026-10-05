// **********************************************************************
// smesh/src/accum_response/NormSumLane.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormSumLane.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace smesh {

namespace {

Acc signedBits(std::uint32_t raw) {
  Acc value = 0;
  static_assert(sizeof(value) == sizeof(raw), "accumulator must be 32 bits");
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

// Original's active iexp polynomial returns zero when data equals maximum. TODO: iexp polynomial is not a mathematical exponent
std::uint32_t iexp(Acc data, Acc maximum, u32 qb_bits, u32 qc_bits) {
  const auto q                 = signedBits(static_cast<std::uint32_t>(data) - static_cast<std::uint32_t>(maximum));
  const auto qb                = signedBits(static_cast<std::uint32_t>(qb_bits));
  const auto qc                = signedBits(static_cast<std::uint32_t>(qc_bits));
  const auto abs_q             = signedBits(q < 0 ? 0u - static_cast<std::uint32_t>(q) : static_cast<std::uint32_t>(q));
  const auto limit             = signedBits(0u - static_cast<std::uint32_t>(qb));
  const auto clipped           = abs_q > limit ? limit : abs_q;
  const auto offset            = static_cast<std::uint32_t>(clipped) + static_cast<std::uint32_t>(qb);
  const auto polynomial        = static_cast<std::uint32_t>(qc) + offset * offset;
  const auto signed_polynomial = q < 0 ? 0u - polynomial : polynomial;
  return static_cast<std::uint32_t>(q) * (signed_polynomial + static_cast<std::uint32_t>(qc));
}

} // namespace

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
      } else if (cmd == NormCmd::SumExp || cmd == NormCmd::InvSumExp) {
        sum += iexp(chunk.data[lane], chunk.max, chunk.igelu_qb, chunk.igelu_qc);
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
