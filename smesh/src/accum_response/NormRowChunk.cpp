// **********************************************************************
// smesh/src/accum_response/NormRowChunk.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026

#include "NormRowChunk.hpp"

#include <cassert>
#include <cstdint>

namespace smesh {

NormRowChunk::NormRowChunk(std::string /*name*/, std::size_t lanes, IMPL_CTOR)
    : lanes_(lanes) {
  assert(lanes_ > 0 && lanes_ <= kDim && (lanes_ & (lanes_ - 1)) == 0);
  UPDATE(update).reads(saved, slot_states, stats).writes(chunk_val, chunk_bits);
}

void NormRowChunk::update() {
  const auto packets  = *saved;
  const auto states   = *slot_states;
  const auto progress = *stats;
  chunk_val           = 0;
  chunk_bits          = NormChunk{};

  for (std::size_t id = 0; id < kNormStatsSlots; ++id) {
    const auto state = static_cast<NormFsmState>(static_cast<std::uint8_t>(states.state[id]));
    const auto left  = static_cast<std::size_t>(static_cast<std::uint16_t>(progress.elems_left[id]));
    if (state != NormFsmState::GetSum || left == 0) continue;

    const auto start = ((left - 1) / lanes_) * lanes_;
    const auto len   = left - start;
    assert(start + len <= kDim);
    NormChunk chunk{};
    chunk.slot = u8(id);
    chunk.len  = u16(len);
    chunk.last = bit(start == 0);
    for (std::size_t lane = 0; lane < len; ++lane) {
      chunk.data[lane] = packets.packet[id].acc_read_resp.data[start + lane];
    }
    chunk_val = 1;
    chunk_bits = chunk;
    return;
  }
}

void NormRowChunk::reset() {
  chunk_val.reset(0);
  chunk_bits.reset(NormChunk{});
}

} // namespace smesh
