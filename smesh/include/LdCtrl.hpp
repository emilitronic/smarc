// **********************************************************************
// smesh/include/LdCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Skeleton for the smesh load controller.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class LdCtrl : public Component {
  DECLARE_COMPONENT(LdCtrl);

 public:
  LdCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshIssue, cmd_in);
  FifoOutput(SmeshRobId, completed);
};

} // namespace smesh
