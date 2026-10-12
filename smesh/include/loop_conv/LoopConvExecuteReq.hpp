// **********************************************************************
// smesh/include/loop_conv/LoopConvExecuteReq.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Execute-job packet shared by the scheduler and worker. No worker state lives here.
*/
#pragma once

#include "LoopConvLdInput.hpp"
#include "LoopConvLdWeight.hpp"

namespace smesh {

// One loop's computation bounds, derived layouts, local-memory bases, and options.
// B's exclusive end address has room to represent the total SPAD row count.
struct LoopConvExecuteReq {
  LoopConvOuterBounds outer_bounds{};
  LoopConvInnerBounds inner_bounds{};
  LoopConvInputGeometry input_geometry{};
  LoopConvWeightGeometry weight_geometry{};
  std::uint32_t a_addr_start = 0;
  std::uint32_t b_addr_end = 0;
  std::uint32_t c_addr_start = 0;
  bit wrot180 = false;
  bit downsample = false;
  std::uint16_t max_pixels_per_row = 0;
  bit input_dilated = false;
  bit trans_weight_0132 = false;
  bit trans_input_3120 = false;
  std::uint8_t loop_id = 0;
};

} // namespace smesh
