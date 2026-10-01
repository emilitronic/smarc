// **********************************************************************
// smesh/include/mvin/MvinPixelRepeater.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Load-path pixel repetition stage. Only pixel_repeats=1 is currently supported.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class MvinPixelRepeater : public Component {
  DECLARE_COMPONENT(MvinPixelRepeater);

 public:
  MvinPixelRepeater(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, in_val);
  Input(DmaReadResp, in_bits);
  Output(bit, in_rdy);
  Output(bit, out_val);
  Output(DmaReadResp, out_bits);
  Input(bit, out_rdy);

  void updateView();
  void updateReady();
  void reset();
};

} // namespace smesh
