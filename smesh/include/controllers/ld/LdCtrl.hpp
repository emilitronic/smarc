// **********************************************************************
// smesh/include/controllers/ld/LdCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class LdCtrlCmdQueue;
class LdCtrlCmdDec;
class LdCtrlState;
class LdCtrlGeom;
class LdCtrlDmaReq;
class LdCtrlCmdTracker;

// Load controller assembled from the independently tested command-path blocks.
class LdCtrl : public Component {
  DECLARE_COMPONENT(LdCtrl);

 public:
  LdCtrl(std::string name, COMPONENT_CTOR);
  ~LdCtrl() override;

  Clock(clk);

  Input(bit,        cmd_val);
  Output(bit,       cmd_rdy);
  Input(SmeshIssue, cmd_bits);

  Output(bit,        dma_req_val);
  Input(bit,         dma_req_rdy);
  Output(DmaReadReq, dma_req_bits);
  Input(bit,               dma_resp_val);  // no rdy backpressure, just update tracker
  Input(DmaReadCompletion, dma_resp_bits); // just cmd_id and bytesRead

  Output(bit,        completed_val);
  Input(bit,         completed_rdy);
  Output(SmeshRsTag, completed_bits);
  Output(bit,        busy);
  Output(u8,         control_state);

  void updateSelectedConfig();
  void updateHeadTag();
  void updateReturnFields();
  void updateBusy();
  void reset();

 private:
  LdCtrlCmdQueue* cmd_queue_ = nullptr;
  LdCtrlCmdDec* decoder_ = nullptr;
  LdCtrlState* state_ = nullptr;
  LdCtrlGeom* geometry_ = nullptr;
  LdCtrlDmaReq* request_ = nullptr;
  LdCtrlCmdTracker* tracker_ = nullptr;

  Output(u64, stride_);
  Output(u32, scale_);
  Output(bit, shrink_);
  Output(u16, block_stride_);
  Output(u8, pixel_repeat_);
  Output(SmeshRsTag, alloc_rs_tag_);
  Output(u16, returned_cmd_id_);
  Output(u32, returned_bytes_read_);
};

} // namespace smesh
