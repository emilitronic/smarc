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

  FifoInput(SmeshIssue, cmd_in);      // RS-issued load command to accept
  FifoOutput(SmeshRobId, completed);  // Let RS know when load is done (rob_id)
  FifoOutput(DmaReadReq, dma_req);    // DMA read request to memory controller

  void updateAccept();
  void reset();

  bool hasActiveCommand() const { return active_valid_; }
  const SmeshIssue& activeCommand() const { return active_; }

 private:
  bool active_valid_ = false;
  SmeshIssue active_{};
};

} // namespace smesh
