// **********************************************************************
// smesh/src/MvinPixelRepeater.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path pixel repetition stage implementation.
*/

#include "MvinPixelRepeater.hpp"

namespace smesh {

MvinPixelRepeater::MvinPixelRepeater(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(data_in).writes(data_out);
}

void MvinPixelRepeater::update() {
  if (data_in.empty() || data_out.full()) {
    return;
  }

  const auto data = data_in.pop();
  assert_always(static_cast<std::uint8_t>(data.pixel_repeats) == 1,
                "MvinPixelRepeater currently supports pixel_repeats=1 only");
  data_out.push(data);
  trace("mvin_pixel_repeater: identity data cmd_id=%u last=%u", static_cast<unsigned>(data.cmd_id), static_cast<unsigned>(data.last));
}

} // namespace smesh
