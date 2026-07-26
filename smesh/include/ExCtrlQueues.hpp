// **********************************************************************
// smesh/include/ExCtrlQueues.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Execute-controller queue components.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

#include <array>
#include <cstddef>

namespace smesh {

constexpr std::size_t kExCtrlCmdWindow = 3; // size of cmd queue multi-head view

class ExCtrlCmdQueue : public Component {
  DECLARE_COMPONENT(ExCtrlCmdQueue);

 public:
  ExCtrlCmdQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshIssue, cmd_in);

  OutputArray(bit, head_val, kExCtrlCmdWindow);         // is valid cmd at this head position?
  OutputArray(SmeshIssue, head_bits, kExCtrlCmdWindow); // cmd at this head position (if valid)
  Input(u8, pop_count); // number of head entries to pop; supported values are 0, 1, or 2

  void updateHeadView();
  void updateStorage();
  void reset();

 private:
  std::array<SmeshIssue, kExCtrlCmdWindow> entries_{};
  std::size_t count_ = 0;
};

} // namespace smesh
