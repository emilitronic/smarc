// **********************************************************************
// smesh/include/accum_response/Normalizer.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
Accumulator normalization skeleton.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class Normalizer : public Component {
  DECLARE_COMPONENT(Normalizer);

 public:
  Normalizer(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,         req_val);
  Output(bit,        req_rdy);
  Input(AccNormReq,  req_bits);

  Output(bit,        resp_val);
  Input(bit,         resp_rdy);
  Output(AccNormReq, resp_bits);

  void updateView();
  void updateReady();
  void updateBuffer();
  void reset() override;

 private:
  Register(bit,        resp_valid_Q_);
  Output(bit,          resp_valid_D_);
  Register(AccNormReq, resp_entry_Q_);
  Output(AccNormReq,   resp_entry_D_);
};

} // namespace smesh
