// **********************************************************************
// smesh/include/controllers/ex/ExCtrlQueues.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Execute-controller queue components.  CONFIG, PRELOAD, and COMPUTE
commands go in this queue.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshConfig.hpp"
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

  Input(bit, cmd_val);
  Output(bit, cmd_rdy);
  Input(SmeshIssue, cmd_bits);

  OutputArray(bit,        head_val,  kExCtrlCmdWindow); // is valid cmd at this head position?
  OutputArray(SmeshIssue, head_bits, kExCtrlCmdWindow); // cmd at this head position (if valid)
  Input(u8, pop_count); // number of head entries to pop; supported values are 0, 1, or 2

  void updateHeadView();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  struct QueueState {
    std::array<SmeshIssue, kDefaultConfig.ex_queue_length> entries{};
    u8 count = 0;
  };

  Output(QueueState, state_Q_);
  Register(QueueState, state_D_);
};

} // namespace smesh
