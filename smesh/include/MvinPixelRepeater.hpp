// **********************************************************************
// smesh/include/MvinPixelRepeater.hpp
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

  FifoInput(DmaReadResp, data_in);
  FifoOutput(DmaReadResp, data_out);

  void update();
};

} // namespace smesh
