// **********************************************************************
// smesh/include/loop_conv/LoopConvLdInputReq.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Request data for the future load-input worker, derived from saved loop bounds.
*/
#pragma once

#include "LoopConvCmdDec.hpp"

namespace smesh {

// Derived input dimensions and SPAD block stride; each field is 16 bits in Original.
struct LoopConvInputGeometry {
  std::uint16_t irows             = 0;
  std::uint16_t icols             = 0;
  std::uint16_t irows_unpadded    = 0;
  std::uint16_t icols_unpadded    = 0;
  std::uint16_t ichs              = 0;
  std::uint16_t input_spad_stride = 0;
};

// One loop slot's input-loading job, including layout and sampling options.
struct LoopConvLdInputReq {
  LoopConvOuterBounds   outer_bounds{};
  LoopConvInnerBounds   inner_bounds{};
  LoopConvInputGeometry geometry{};
  std::uint32_t         addr_start         = 0;
  std::uint64_t         dram_addr          = 0;
  bit                   downsample         = false;
  std::uint16_t         max_pixels_per_row = 0;
  bit                   input_dilated      = false;
  bit                   trans_input_3120   = false;
  std::uint8_t          loop_id            = 0;
};

} // namespace smesh
