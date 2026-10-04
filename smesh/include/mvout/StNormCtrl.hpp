// **********************************************************************
// smesh/include/mvout/StNormCtrl.hpp
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

  InputArray(bit,           accum_read_resp_val, kAccBanks);
  OutputArray(bit,          accum_read_resp_rdy, kAccBanks);
  InputArray(AccumReadResp, accum_read_resp_bits, kAccBanks);

  Input(bit,                norm_deq_val);
  Output(bit,               norm_deq_rdy);
  Input(DmaWriteReq,        norm_deq_bits); // store metadata from write norm queue

  Output(bit,               scale_enq_val);
  Input(bit,                scale_enq_rdy);

  Output(bit,               normalizer_cmd_val);
  Input(bit,                normalizer_cmd_rdy);
  Output(AccNormReq,        normalizer_req_bits);

  void updateRequest();
  void updateHandshake();

 private:
  Output(u8, selected_bank_);
};

} // namespace smesh
