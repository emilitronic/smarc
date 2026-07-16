// **********************************************************************
// smesh/include/ExCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Structural shell for the smesh execute controller.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class ExCtrl : public Component {
  DECLARE_COMPONENT(ExCtrl);

 public:
  ExCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshIssue, cmd_in);
  FifoOutput(SmeshRsTag, completed);

  Output(bit, spad_read_req_val);
  Input(bit, spad_read_req_rdy);
  Output(SpadReadReq, spad_read_req_bits);
  Input(bit, spad_read_resp_val);
  Input(SpadReadResp, spad_read_resp_bits);
  Output(bit, spad_read_resp_rdy);

  Output(bit, accum_read_req_val);
  Input(bit, accum_read_req_rdy);
  Output(AccumReadReq, accum_read_req_bits);
  Input(bit, accum_read_resp_val);
  Input(AccumReadResp, accum_read_resp_bits);
  Output(bit, accum_read_resp_rdy);

  Output(bit, spad_write_val);
  Input(bit, spad_write_rdy);
  Output(DmaReadResp, spad_write_bits);

  Output(bit, accum_write_val);
  Input(bit, accum_write_rdy);
  Output(DmaReadResp, accum_write_bits);

  void updateCmdSink();
  void updateReadPorts();
  void updateWritePorts();
  void reset();
};

} // namespace smesh
