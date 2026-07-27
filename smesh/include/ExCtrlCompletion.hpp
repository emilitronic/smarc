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

  Input(bit, config_val);               // FSM accepted a CONFIG command
  Input(bit, config_rs_tag_valid);
  Input(SmeshRsTag, config_rs_tag);
  FifoOutput(SmeshRsTag, completed);    // selected execute completion back toward RS
  Output(bit, pending_completed_valid); // any pending completion register is occupied

  void updatePendingView();
  void updateConfigCompletion();
  void reset();

 private:
  bool pending_completed_valid_[2] = {false, false};
  SmeshRsTag pending_completed_bits_[2] = {0, 0};
};

} // namespace smesh
