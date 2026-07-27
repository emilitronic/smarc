// **********************************************************************
// smesh/include/ArbComplete.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 25 2026
/*
Completion arbiter for controller-to-RS completion tags.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class ArbExLdStComplete : public Component {
  DECLARE_COMPONENT(ArbExLdStComplete);

 public:
  ArbExLdStComplete(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, ex_completed_val);
  Input(SmeshRsTag, ex_completed_bits);
  FifoInput(SmeshRsTag, ld_completed);
  FifoInput(SmeshRsTag, st_completed);

  FifoOutput(SmeshRsTag, rs_completed);

  void update();
};

} // namespace smesh
