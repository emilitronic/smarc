// **********************************************************************
// smesh/include/LdCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Load controller declaration.
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

  void updateAccept();
  void reset();

  bool hasActiveCommand() const { return active_valid_; }
  const SmeshIssue& activeCommand() const { return active_; }

 private:
  bool active_valid_ = false;
  SmeshIssue active_{};
};

} // namespace smesh
