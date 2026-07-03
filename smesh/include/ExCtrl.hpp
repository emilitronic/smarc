// **********************************************************************
// smesh/include/ExCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Skeleton for the smesh execute controller.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class ExCtrl : public Component {
  DECLARE_COMPONENT(ExCtrl);

 public:
  ExCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshIssue, cmd_in);
  FifoOutput(SmeshRobId, completed);

  void update();
  void reset();

 private:
  bool active_valid_ = false;
  SmeshRobId active_rob_id_ = 0;
};

} // namespace smesh
