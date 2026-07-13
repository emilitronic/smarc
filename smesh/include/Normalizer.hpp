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

  Input(bit, req_val);
  Output(bit, req_rdy);
  Input(AccNormReq, req_bits);
  FifoOutput(AccNormReq, req_out);

  void updateReady();
  void update();
};

} // namespace smesh
