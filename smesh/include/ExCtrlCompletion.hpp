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
  Input(bit, config_rs_tag_valid);      // FSM cmd rs_tag_valid for CONFIG command
  Input(SmeshRsTag, config_rs_tag);     // FSM cmd rs_tag for CONFIG command
  Output(bit, completed_val);           // selected execute completion valid
  Output(SmeshRsTag, completed_bits);   // selected execute completion tag
  Output(bit, pending_completed_valid); // any pending completion register is occupied

  void updatePendingView();
  void updateConfigCompletion();
  void reset();

 private:
  bool pending_completed_valid_[2] = {false, false};
  SmeshRsTag pending_completed_bits_[2] = {0, 0};

  // TODO
  // complete_bits_count
};

} // namespace smesh
