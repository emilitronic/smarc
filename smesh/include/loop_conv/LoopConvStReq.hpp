// **********************************************************************
// smesh/include/loop_conv/LoopConvStReq.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Store-job packet shared with the scheduler; the worker is added separately.
*/
#pragma once

#include "LoopConvCmdDec.hpp"

namespace smesh {

// One loop's output bounds, accumulator base, DRAM destination, and store options.
// ochs is the only field of Original's derived-parameter bundle used by Store.
struct LoopConvStReq {
  LoopConvOuterBounds outer_bounds{};
  LoopConvInnerBounds inner_bounds{};
  std::uint16_t ochs = 0;
  std::uint32_t addr_start = 0;
  std::uint64_t dram_addr = 0;
  bit no_pool = false;
  std::uint8_t activation = 0; // Two-bit activation encoding in Original.
  bit trans_output_1203 = false;
  std::uint8_t loop_id = 0;
};

} // namespace smesh
