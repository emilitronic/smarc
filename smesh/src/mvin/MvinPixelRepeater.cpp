// **********************************************************************
// smesh/src/mvin/MvinPixelRepeater.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path pixel repetition stage implementation.
*/

#include "MvinPixelRepeater.hpp"

namespace smesh {

MvinPixelRepeater::MvinPixelRepeater(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateView).reads(in_val, in_bits).writes(out_val, out_bits);
  UPDATE(updateReady).reads(out_rdy).writes(in_rdy);
}

void MvinPixelRepeater::updateView() {
  if (in_val == 1) {
    assert_always(static_cast<std::uint8_t>(in_bits->pixel_repeats) == 1,
                  "MvinPixelRepeater currently supports pixel_repeats=1 only");
  }
  out_val = in_val;
  out_bits = in_val == 1 ? *in_bits : DmaReadResp{};
}

void MvinPixelRepeater::updateReady() {
  in_rdy = out_rdy;
}

void MvinPixelRepeater::reset() {
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(DmaReadResp{});
}

} // namespace smesh
