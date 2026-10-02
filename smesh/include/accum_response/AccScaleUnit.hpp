// **********************************************************************
// smesh/include/accum_response/AccScaleUnit.hpp
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

  Input(bit,         req_val);
  Output(bit,        req_rdy);
  Input(AccScaleReq, req_bits);

  Output(bit,          out_val);
  Output(AccScaleResp, out_bits);

  Input(bit, out_rdy_issue);
  Input(bit, out_rdy_exresp);

  void updateView();
  void updateReady();
  void updateBuffer();
  void reset() override;

 private:
  Register(bit,          out_valid_Q_);
  Output(bit,            out_valid_D_);
  Register(AccScaleResp, out_entry_Q_);
  Output(AccScaleResp,   out_entry_D_);
};

} // namespace smesh
