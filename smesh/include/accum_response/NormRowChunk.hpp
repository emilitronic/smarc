// **********************************************************************
// smesh/include/accum_response/NormRowChunk.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Selects row chunks
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

#include <cstddef>

namespace smesh {

// Selects one chunk from a slot in the requested reduction state (sum or max).
class NormRowChunk : public Component {
  DECLARE_COMPONENT(NormRowChunk);

 public:
  NormRowChunk(std::string name, std::size_t lanes, NormFsmState target_state, COMPONENT_CTOR);

  Clock(clk);

  Input(NormSavedPackets, saved);
  Input(NormStateRegs,    slot_states);
  Input(NormStatsRegs,    stats);

  Output(bit,             chunk_val);
  Output(NormChunk,       chunk_bits);

  void update();
  void reset() override;

 private:
  std::size_t lanes_;
  NormFsmState target_state_;
};

} // namespace smesh
