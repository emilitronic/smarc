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
  Output(bit, deq_val);          // explicit dequeue-side view of the head entry
  Output(DmaWriteReq, deq_bits); // explicit dequeue-side view of the head entry
  Input(bit, deq_rdy);           // external control says the head entry may advance
  // expose (show head entry) and pop (consum head entry) need separate update fns. 
  // to avoid combinational loop between deq_val and deq_rdy
  // otherwise the same function will produce signal that starts
  // the decision and consume the signal that comes back from
  // the decision (a combinational cycle)
  void updateDeqView(); 
  void updateDeqPop();  
};

class DmaWriteNormQueue : public Component {
  DECLARE_COMPONENT(DmaWriteNormQueue);

 public:
  DmaWriteNormQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, enq_val);           // wires tapped off tail of queue for external control
  Input(DmaWriteReq, enq_bits);  // wires tapped off tail of queue for external control
  Output(bit, enq_rdy);          // wires tapped off tail of queue for external control
  Output(bit, deq_val);
  Output(DmaWriteReq, deq_bits);
  Input(bit, deq_rdy);

  void updateEnqReady();
  void updateEnqAccept();
  void updateDeqView();
  void updateDeqPop();
  void reset();

 private:
  bool valid_ = false;
  DmaWriteReq entry_{};
};

class DmaWriteScaleQueue : public Component {
  DECLARE_COMPONENT(DmaWriteScaleQueue);

 public:
  DmaWriteScaleQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, enq_val);
  Input(DmaWriteReq, enq_bits);
  Output(bit, enq_rdy);
  Output(bit, deq_val);
  Output(DmaWriteReq, deq_bits);
  Input(bit, deq_rdy);

  void updateEnqReady();
  void updateEnqAccept();
  void updateDeqView();
  void updateDeqPop();
  void reset();

 private:
  bool valid_ = false;
  DmaWriteReq entry_{};
};

class DmaWriteIssueQueue : public Component {
  DECLARE_COMPONENT(DmaWriteIssueQueue);

 public:
  DmaWriteIssueQueue(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, enq_val);
  Input(DmaWriteReq, enq_bits);
  Output(bit, enq_rdy);
  Output(bit, deq_val);
  Output(DmaWriteReq, deq_bits);
  Input(bit, deq_rdy);

  void updateEnqReady();
  void updateEnqAccept();
  void updateDeqView();
  void updateDeqPop();
  void reset();

 private:
  bool valid_ = false;
  DmaWriteReq entry_{};
};

} // namespace smesh
