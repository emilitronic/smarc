// **********************************************************************
// smesh/include/StCtrlCmdTracker.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
/*
Tracks the response count and RS tag for each in-flight store command.
  Key behavior:

  - Two configurable tracker entries.
  - Allocation chooses the first free entry.
  - Stores the RS tag and expected response count.
  - Each returned response decrements the selected entry.
  - Zero remaining responses generates completion.
  - Completion remains asserted until accepted.
  - Full tracker backpressures new allocations.
  - Entries become reusable after completion.

Allocation reserves one entry for an entire command. Every store response
decrements that command's remaining count. A zero count produces one RS
completion and the entry is released when that completion is accepted.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace smesh {

static_assert(kStoreCmdTrackerEntries > 0,
              "StoreController command tracker must have at least one entry");

struct StCtrlCmdTrackerEntry {
  bit valid = 0;
  SmeshRsTag rs_tag = 0;
  std::uint32_t responses_left = 0;
};

struct StCtrlCmdTrackerState {
  std::array<StCtrlCmdTrackerEntry, kStoreCmdTrackerEntries> entries{};
};

class StCtrlCmdTracker : public Component {
  DECLARE_COMPONENT(StCtrlCmdTracker);

 public:
  StCtrlCmdTracker(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,         alloc_val);
  Output(bit,        alloc_rdy);
  Input(u32,         alloc_response_count);
  Input(SmeshRsTag,  alloc_rs_tag);
  Output(u16,        alloc_cmd_id);

  Input(bit,         returned_val);
  Output(bit,        returned_rdy);
  Input(u16,         returned_cmd_id);
  Input(u32,         returned_response_count);

  Output(bit,        completed_val);
  Input(bit,         completed_rdy);
  Output(SmeshRsTag, completed_bits);
  Output(u16,        completed_cmd_id);

  void updateView();
  void updateState();
  void reset();

 private:
  Output(StCtrlCmdTrackerState, tracker_Q_);
  Register(StCtrlCmdTrackerState, tracker_D_);
};

} // namespace smesh
