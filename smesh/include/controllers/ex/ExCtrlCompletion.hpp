// **********************************************************************
// smesh/include/controllers/ex/ExCtrlCompletion.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 27 2026
/*
Execute-controller completion bookkeeping.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstddef>

namespace smesh {

struct ExCtrlPendingCompletionState {
  std::array<bit, 2>        val{};
  std::array<SmeshRsTag, 2> bits{};
};

class ExCtrlCompletion : public Component {
  DECLARE_COMPONENT(ExCtrlCompletion);

 public:
  ExCtrlCompletion(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,        config_val);                         // FSM accepted a CONFIG command
  Input(bit,        config_rs_tag_val);                  // FSM cmd rs_tag_valid for CONFIG command
  Input(SmeshRsTag, config_rs_tag);                      // FSM cmd rs_tag for CONFIG command

  // Pending completions are RS completions for commands whose tags will not return through
  // Mesher/writeback and are waiting to be reported on ExCtrl's completion port.
  // It is pending only until completion gets an available cycle to report it.
  // So we try to report completion on these commands after they issue (not after they complete).
  InputArray(bit,        pending_completed_set_val,  2); // FSM writes pending slot valid values
  InputArray(SmeshRsTag, pending_completed_set_bits, 2); // FSM writes pending slot tags

  Input(bit,        mesh_completed_rs_tag_fire);         // writeback reports a mesh completion
  Input(SmeshRsTag, mesh_completed_bits);                // completed mesh operation tag

  Output(bit,        completed_val);  // selected execute completion valid
  Output(SmeshRsTag, completed_bits); // selected execute completion tag
  Output(bit, pending_completed_val); // any pending completion register is occupied

  void updatePendingView();
  void updateCompletionView();
  void updatePendingState();
  void reset();

 private:
  Output(ExCtrlPendingCompletionState,   pending_completed_Q_); // committed pending slots
  Register(ExCtrlPendingCompletionState, pending_completed_D_); // pending slots for next cycle

  static constexpr std::size_t kPendingEntries = 2;
};

} // namespace smesh
