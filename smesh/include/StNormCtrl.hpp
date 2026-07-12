// **********************************************************************
// smesh/include/StNormCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Store-path normalization-stage control.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StNormCtrl : public Component {
  DECLARE_COMPONENT(StNormCtrl);

 public:
  StNormCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Output(bit, norm_deq_rdy);
  Output(bit, scale_enq_val);
  Output(bit, normalizer_cmd_val);
  Output(bit, accum_read_resp_rdy);

  void update();
};

} // namespace smesh
