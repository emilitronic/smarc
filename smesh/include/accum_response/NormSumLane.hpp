// **********************************************************************
// smesh/include/accum_response/NormSumLane.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Adds values for Sum and Mean. For Variance and InvStddev, adds squared
differences from the saved mean. For SumExp, adds each element's iexp result.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Reduces one selected row chunk to a 32-bit partial sum.
class NormSumLane : public Component {
  DECLARE_COMPONENT(NormSumLane);

 public:
  NormSumLane(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,            chunk_val);
  Input(NormChunk,      chunk_bits);

  Output(bit,           result_val);
  Output(NormSumResult, result_bits);

  void update();
  void reset() override;
};

} // namespace smesh
