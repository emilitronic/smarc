// **********************************************************************
// smesh/include/Normalizer.hpp
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

  FifoInput(AccumReadResp, acc_resp_in);
  FifoInput(DmaWriteReq, cmd_in);
  FifoOutput(AccNormReq, req_out);

  void update();
};

} // namespace smesh
