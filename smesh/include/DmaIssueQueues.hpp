// **********************************************************************
// smesh/include/DmaIssueQueues.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 10 2026
/*
DMA issue queue components.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class DmaReadIssueQueue : public Component {
  DECLARE_COMPONENT(DmaReadIssueQueue);

 public:
  DmaReadIssueQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaReadReq, req_in);
  FifoOutput(DmaReadReq, req_out);

  void update();
};

class DmaWriteIssueQueue : public Component {
  DECLARE_COMPONENT(DmaWriteIssueQueue);

 public:
  DmaWriteIssueQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaWriteReq, req_in);
  FifoOutput(DmaWriteReq, req_out);

  void update();
};

} // namespace smesh
