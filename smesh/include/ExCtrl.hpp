// **********************************************************************
// smesh/include/ExCtrl.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 1 2026
/*
Structural shell for the smesh execute controller.

*/
#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlCompletion.hpp"
#include "ExCtrlDecoder.hpp"
#include "ExCtrlQueues.hpp"
#include "ExCtrlState.hpp"
#include "SmeshPorts.hpp"
#include "SmeshTypes.hpp"

namespace smesh {

class ExCtrl : public Component {
  DECLARE_COMPONENT(ExCtrl);

 public:
  ExCtrl(std::string name, COMPONENT_CTOR);
  ~ExCtrl() override;

  Clock(clk);

  FifoInput(SmeshIssue, cmd_in);
  Output(bit, completed_val);
  Output(SmeshRsTag, completed_bits);

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
  InputArray(ExCtrlAccumReadResp, accum_read_resp_bits, kAccBanks);
  OutputArray(bit, accum_read_resp_rdy, kAccBanks);

  Output(bit, spad_write_val);
  Input(bit, spad_write_rdy);
  Output(DmaReadResp, spad_write_bits);

  Output(bit, accum_write_val);
  Input(bit, accum_write_rdy);
  Output(DmaReadResp, accum_write_bits);

  void updateReadPorts();
  void updateWritePorts();
  void updateDecoderInputs();
  void reset();

 private:
  ExCtrlCmdQueue* cmd_queue_  = nullptr;
  ExCtrlCompletion* completion_ = nullptr;
  ExCtrlDecoder* cmd_decoder_ = nullptr;
  ExCtrlState* cmd_state_     = nullptr;
  Output(bit, decoder_ex_read_from_acc_);
  Output(bit, decoder_ex_write_to_spad_);
  Output(bit, mesh_matmul_in_progress_);
};

} // namespace smesh
