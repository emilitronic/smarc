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
#include "ExCtrlFeedSignals.hpp"
#include "ExCtrlMeshCntlDeqCtrl.hpp"
#include "ExCtrlMeshCntlPack.hpp"
#include "ExCtrlMeshInSelPad.hpp"
#include "ExCtrlMeshTagSelect.hpp"
#include "ExCtrlOperandPack.hpp"
#include "ExCtrlQueues.hpp"
#include "ExCtrlReadPriority.hpp"
#include "ExCtrlReadReqLogic.hpp"
#include "ExCtrlRowAddr.hpp"
#include "ExCtrlRowFeedState.hpp"
#include "ExCtrlRowPad.hpp"
#include "ExCtrlState.hpp"
#include "Mesher.hpp"
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

  OutputArray(bit, spad_write_val, kSpBanks);
  InputArray(bit, spad_write_rdy, kSpBanks);
  OutputArray(DmaReadResp, spad_write_bits, kSpBanks);

  OutputArray(bit, accum_write_val, kAccBanks);
  InputArray(bit, accum_write_rdy, kAccBanks);
  OutputArray(DmaReadResp, accum_write_bits, kAccBanks);

  void updateReadPorts();
  void updateWritePorts();
  void updateDecoderInputs();
  void reset();

 private:
  ExCtrlCmdQueue* cmd_queue_    = nullptr;
  ExCtrlCompletion* completion_ = nullptr;
  ExCtrlDecoder* cmd_decoder_   = nullptr;
  ExCtrlState* cmd_state_       = nullptr;
  ExCtrlRowAddr* cmd_rowaddr_   = nullptr;
  ExCtrlRowPad* cmd_rowpad_     = nullptr;
  ExCtrlOperandPack* op_pack_   = nullptr;
  ExCtrlRowFeedState* row_feed_ = nullptr;
  ExCtrlMeshTagSelect* tag_select_ = nullptr;
  ExCtrlReadPriority* read_prio_ = nullptr;
  ExCtrlReadReqLogic* rd_req_ = nullptr;
  ExCtrlFeedSignals* feed_signals_ = nullptr;
  ExCtrlMeshCntlPack* mesh_cntl_pack_ = nullptr;
  ExCtrlMeshCntlQueue* mesh_cntl_queue_ = nullptr;
  ExCtrlMeshInSelPad* mesh_in_sel_pad_ = nullptr;
  ExCtrlMeshCntlDeqCtrl* mesh_cntl_deq_ctrl_ = nullptr;
  Mesher* mesher_ = nullptr;
  Output(bit, decoder_ex_read_from_acc_);
  Output(bit, decoder_ex_write_to_spad_);
  Output(bit, writeback_ex_write_to_acc_);
  Output(u32, writeback_aligned_to_);
  Output(bit, mesh_cntl_pack_perform_mul_pre_);
  Output(bit, tag_select_performing_single_mul_);
  Output(bit, im2col_wire_);
  Output(bit, im2col_en_);
  Output(bit, im2colling_);
  Output(bit, cntl_rdy_);
  Output(u32, row_addr_block_size_);
};

} // namespace smesh
