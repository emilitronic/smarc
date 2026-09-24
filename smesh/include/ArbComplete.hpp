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

  Input(bit,        ex_completed_val);
  Input(SmeshRsTag, ex_completed_bits);
  Input(bit,        ld_completed_val);
  Input(SmeshRsTag, ld_completed_bits);
  Output(bit,       ld_completed_rdy);
  Input(bit,        st_completed_val);
  Input(SmeshRsTag, st_completed_bits);
  Output(bit,       st_completed_rdy);

  FifoOutput(SmeshRsTag, rs_completed);

  void update();
  void reset();
};

} // namespace smesh
