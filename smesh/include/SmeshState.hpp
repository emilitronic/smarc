// **********************************************************************
// smesh/include/SmeshState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 26 2026
/*
Internal model state.  Scratchpad, accumulator, and PE sizing.  
*/
#pragma once

#include "SmeshTypes.hpp"

#include <array>
#include <cstddef>

namespace smesh {

struct SmeshState {
  using SpadRow = std::array<Elem, kDim>; // cols (elements) in a SP row
  using AccRow = std::array<Acc, kDim>;
  // size internal memory and computing arrays
  std::array<SpadRow, kScratchpadRows> spad{};
  std::array<AccRow, kAccumulatorRows> accumulator{};
  std::array<AccRow, kDim> pe_state{};
  // metadata for data location and shape
  std::uint32_t preload_sp_row = 0;
  std::uint32_t output_acc_row = 0;
  MatrixShape preload_shape{};
  MatrixShape output_shape{};

  void reset();
};

} // namespace smesh
