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

class DmaWriteDispatchQueue : public Component {
  DECLARE_COMPONENT(DmaWriteDispatchQueue);

 public:
  DmaWriteDispatchQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaWriteReq, req_in);
  FifoOutput(DmaWriteReq, req_out);
  Output(bit, front_valid);        // wires tapped off head of queue for external control
  Output(DmaWriteReq, front_bits); // wires tapped off head of queue for external control
  Input(bit, front_ready);         // wires tapped off head of queue for external control

  void update();
};

class DmaWriteNormQueue : public Component {
  DECLARE_COMPONENT(DmaWriteNormQueue);

 public:
  DmaWriteNormQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaWriteReq, req_in);
  FifoOutput(DmaWriteReq, req_out);

  void update();
};

class DmaWriteScaleQueue : public Component {
  DECLARE_COMPONENT(DmaWriteScaleQueue);

 public:
  DmaWriteScaleQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(DmaWriteReq, req_in);
  FifoOutput(DmaWriteReq, req_out);

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
