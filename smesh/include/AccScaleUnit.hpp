// **********************************************************************
// smesh/include/AccScaleUnit.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Accumulator scale-stage skeleton.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class AccScaleUnit : public Component {
  DECLARE_COMPONENT(AccScaleUnit);

 public:
  AccScaleUnit(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, req_val);
  Output(bit, req_rdy);
  Input(AccScaleReq, req_bits);
  FifoOutput(AccScaleResp, resp_out);

  void updateReady();
  void update();
};

} // namespace smesh
