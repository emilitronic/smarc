// **********************************************************************
// smesh/include/StCtrl2.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026
// StoreController composition with command decode, FSM, DMA requests, and tracking.
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class StCtrlCmdQueue;
class StCtrlCmdDec;
class StCtrlGeom;
class StCtrlDmaReq;
class StCtrlCmdTracker;
class StCtrlState;

class StCtrl2 : public Component {
  DECLARE_COMPONENT(StCtrl2);

 public:
  StCtrl2(std::string name, COMPONENT_CTOR);
  ~StCtrl2() override;

  Clock(clk);

  Input(bit,        cmd_val);
  Output(bit,       cmd_rdy);
  Input(SmeshIssue, cmd_bits);
  Output(bit,         dma_req_val);
  Input(bit,          dma_req_rdy);
  Output(DmaWriteReq, dma_req_bits);
  Input(bit,          dma_resp_val);
  Output(bit,         dma_resp_rdy);
  Input(DmaWriteResp, dma_resp_bits);
  Output(bit,        completed_val);
  Input(bit,         completed_rdy);
  Output(SmeshRsTag, completed_bits);
  Output(u8, control_state);

  void updateResponseFields();
  void reset();

 private:
  StCtrlCmdQueue* cmd_queue_ = nullptr;
  StCtrlCmdDec* decoder_ = nullptr;
  StCtrlGeom* geometry_ = nullptr;
  StCtrlDmaReq* request_ = nullptr;
  StCtrlCmdTracker* tracker_ = nullptr;
  StCtrlState* state_ = nullptr;

  Output(u16, returned_cmd_id_);
  Output(u32, returned_response_count_);
};

} // namespace smesh
