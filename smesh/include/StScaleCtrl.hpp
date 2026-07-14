// **********************************************************************
// smesh/include/StScaleCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-path scale-stage control.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StScaleCtrl : public Component {
  DECLARE_COMPONENT(StScaleCtrl);

 public:
  StScaleCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, scale_deq_val);
  Input(DmaWriteReq, scale_deq_bits);
  Input(bit, normalizer_resp_val);
  Input(AccNormReq, normalizer_resp_bits);
  Input(bit, acc_scale_req_rdy);
  Input(bit, issue_enq_rdy);

  Output(bit, scale_deq_rdy);
  Output(bit, normalizer_resp_rdy);
  Output(bit, acc_scale_req_val);
  Output(AccScaleReq, acc_scale_req_bits);
  Output(bit, issue_enq_val);

  void update();
};

} // namespace smesh
