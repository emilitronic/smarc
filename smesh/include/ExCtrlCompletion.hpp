// **********************************************************************
// smesh/include/ExCtrlCompletion.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 27 2026
/*
Execute-controller completion bookkeeping shell.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class ExCtrlCompletion : public Component {
  DECLARE_COMPONENT(ExCtrlCompletion);

 public:
  ExCtrlCompletion(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Output(bit, pending_completed_valid);    // any pending completion register is occupied

  void update();
  void reset();

 private:
  bool pending_completed_valid_[2] = {false, false};
  SmeshRsTag pending_completed_bits_[2] = {0, 0};
};

} // namespace smesh
