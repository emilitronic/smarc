// **********************************************************************
// smesh/include/LdCtrlCmdTracker.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026
/*
The tracker accepts a computed byte count, accounts for returned bytes by 
command ID, and retains a completion until it is accepted.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstdint>

namespace smesh {

static_assert(kLoadCmdTrackerEntries > 0);

struct LdCtrlCmdTrackerEntry {
  bit valid = 0;
  SmeshRsTag rs_tag = 0;
  std::uint32_t bytes_left = 0;
};

struct LdCtrlCmdTrackerState {
  std::array<LdCtrlCmdTrackerEntry, kLoadCmdTrackerEntries> entries{};
};

// Holds each load until all DMA bytes return and its completion is accepted.
class LdCtrlCmdTracker : public Component {
  DECLARE_COMPONENT(LdCtrlCmdTracker);

 public:
  LdCtrlCmdTracker(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,        alloc_val);
  Output(bit,       alloc_rdy);
  Input(u32,        alloc_bytes_to_read);
  Input(SmeshRsTag, alloc_rs_tag);
  Output(u16,       alloc_cmd_id);

  Input(bit, returned_val);
  Input(u16, returned_cmd_id);
  Input(u32, returned_bytes_read);

  Output(bit,        completed_val);
  Input(bit,         completed_rdy);
  Output(SmeshRsTag, completed_bits);
  Output(u16,        completed_cmd_id);
  Output(bit,        busy);

  void updateView();
  void updateState();
  void reset();

 private:
  Output(LdCtrlCmdTrackerState, tracker_Q_);
  Register(LdCtrlCmdTrackerState, tracker_D_);
};

} // namespace smesh
