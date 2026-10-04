// **********************************************************************
// smesh/include/accum_response/NormMaxLane.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Reduces each chunk to a single signed maximum value.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Finds the largest signed element in one selected row chunk.
class NormMaxLane : public Component {
  DECLARE_COMPONENT(NormMaxLane);

 public:
  NormMaxLane(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, chunk_val);
  Input(NormChunk, chunk_bits);

  Output(bit, result_val);
  Output(NormMaxResult, result_bits);

  void update();
  void reset() override;
};

} // namespace smesh
