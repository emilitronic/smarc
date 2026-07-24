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
#include "SmeshTypes.hpp"

namespace smesh {

class ExCtrl : public Component {
  DECLARE_COMPONENT(ExCtrl);

 public:
  ExCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  FifoInput(SmeshIssue, cmd_in);
  FifoOutput(SmeshRsTag, completed);

  OutputArray(bit, spad_read_req_val, kSpBanks);
  InputArray(bit, spad_read_req_rdy, kSpBanks);
  OutputArray(SpadReadReq, spad_read_req_bits, kSpBanks);
  InputArray(bit, spad_read_resp_val, kSpBanks);
  InputArray(SpadReadResp, spad_read_resp_bits, kSpBanks);
  OutputArray(bit, spad_read_resp_rdy, kSpBanks);

  OutputArray(bit, accum_read_req_val, kAccBanks);
  InputArray(bit, accum_read_req_rdy, kAccBanks);
  OutputArray(AccumReadReq, accum_read_req_bits, kAccBanks);
  InputArray(bit, accum_read_resp_val, kAccBanks);
  InputArray(AccumReadResp, accum_read_resp_bits, kAccBanks);
  OutputArray(bit, accum_read_resp_rdy, kAccBanks);

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
