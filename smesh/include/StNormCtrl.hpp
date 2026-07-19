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

  Input(bit, norm_deq_val);
  Input(DmaWriteReq, norm_deq_bits);
  InputArray(bit, accum_read_resp_val, kAccBanks);
  InputArray(AccumReadResp, accum_read_resp_bits, kAccBanks);
  Input(bit, normalizer_cmd_rdy);
  Input(bit, scale_enq_rdy);

  Output(bit, norm_deq_rdy);
  Output(bit, scale_enq_val);
  Output(bit, normalizer_cmd_val);
  Output(AccNormReq, normalizer_req_bits);
  OutputArray(bit, accum_read_resp_rdy, kAccBanks);

  void update();
};

} // namespace smesh
