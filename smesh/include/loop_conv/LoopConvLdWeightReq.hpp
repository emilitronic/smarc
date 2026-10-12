// **********************************************************************
// smesh/include/loop_conv/LoopConvLdWeightReq.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Request format shared by the scheduler and load-weights worker. No state lives here.
*/
#pragma once

#include "LoopConvCmdDec.hpp"

namespace smesh {

// Weight-layout fields consumed by the worker; Original stores each in 16 bits.
// The *_per_bank names mean DIM-sized channel groups, not physical SPAD banks.
struct LoopConvWeightGeometry {
  std::uint16_t ochs                  = 0;
  std::uint16_t out_channels_per_bank = 0;
  std::uint16_t in_channels_per_bank  = 0;
  std::uint16_t weight_spad_stride    = 0;
};

// One loop slot's weight-loading job. Weights occupy rows immediately before
// addr_end, an exclusive SPAD end address which can equal the total row count.
struct LoopConvLdWeightReq {
  LoopConvOuterBounds    outer_bounds{};
  LoopConvInnerBounds    inner_bounds{};
  LoopConvWeightGeometry geometry{};
  std::uint32_t          addr_end          = 0;
  std::uint64_t          dram_addr         = 0;
  bit                    trans_weight_1203 = false;
  bit                    trans_weight_0132 = false;
  bit                    dw                = false;
  std::uint8_t           loop_id           = 0;
};

} // namespace smesh
