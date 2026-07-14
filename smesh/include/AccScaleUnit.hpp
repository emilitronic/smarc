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
  Output(bit, out_val);
  Output(AccScaleResp, out_bits);
  Input(bit, out_rdy);

  void updateReady();
  void updateOutView();
  void updateOutPop();
  void update();

 private:
  bool out_valid_ = false;
  AccScaleResp out_entry_{};
};

} // namespace smesh
