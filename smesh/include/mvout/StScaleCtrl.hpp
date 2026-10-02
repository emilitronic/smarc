// **********************************************************************
// smesh/include/mvout/StScaleCtrl.hpp
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
 
  Input(bit,          normalizer_resp_val);
  Output(bit,         normalizer_resp_rdy);
  Input(AccNormReq,   normalizer_resp_bits);

  Input(bit,          scale_deq_val);
  Output(bit,         scale_deq_rdy);
  Input(DmaWriteReq,  scale_deq_bits);

  Output(bit,         acc_scale_req_val);
  Input(bit,          acc_scale_req_rdy);
  Output(AccScaleReq, acc_scale_req_bits);

  Output(bit,         issue_enq_val);
  Input(bit,          issue_enq_rdy);

  void update();
};

} // namespace smesh
