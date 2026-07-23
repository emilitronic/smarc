// **********************************************************************
// smesh/include/SmeshTop.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 8 2026
/*
Top-level smesh composition point.

This component starts empty on purpose. We will add the RS, controllers, DMA
path, and local memories incrementally so the hardware block structure stays
easy to inspect.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include <array>

#include "Accum.hpp"
#include "AccScaleUnit.hpp"
#include "ArbReadLocal.hpp"
#include "ArbWriteLocal.hpp"
#include "DmaIssueQueues.hpp"
#include "DmaReadCompletionMux.hpp"
#include "DmaReader.hpp"
#include "DmaWriter.hpp"
#include "ExCtrl.hpp"
#include "LdCtrl.hpp"
#include "MvinLocalRouter.hpp"
#include "MvinPixelRepeater.hpp"
#include "MvinScale.hpp"
#include "Normalizer.hpp"
#include "SmeshCmdQueues.hpp"
#include "SmeshRS.hpp"
#include "Spad.hpp"
#include "SpadReadPipes.hpp"
#include "SpadWriter.hpp"
#include "StCtrl.hpp"
#include "StIssueCtrl.hpp"
#include "StIssueMux.hpp"
#include "StNormCtrl.hpp"
#include "StReadCtrl.hpp"
#include "StScaleCtrl.hpp"
#include "WriteCtrl.hpp"
#include "smem/MemTypes.hpp"

namespace smesh {

class SmeshTop : public Component {
  DECLARE_COMPONENT(SmeshTop);

 public:
  SmeshTop(std::string name, COMPONENT_CTOR);
  ~SmeshTop() override;

  Clock(clk);

  Input(bit, cmd_valid);
  Input(SmeshCmd, cmd_bits);
  Output(bit, cmd_ready);

  // Memory accessors let the testbench connect the current memory boundary.
  auto& memReq() { return dma_reader_->mem_req; }        // when top.memReq() is called, give them mem_req of dma_reader_
  auto& memResp() { return dma_reader_->mem_resp; }      // when top.memResp() is called, give them mem_resp of dma_reader_

  // narrow inspection accessors for testbench to check internal state
  const SmeshRS& rs()     const { return *rs_; }
  const LdCtrl&  ldCtrl() const { return *ld_ctrl_; }
  const Spad&    spad()   const { return *spad_; }
  const SpadDmaReadPipe& spadDmaReadPipe() const { return *spad_dma_read_pipe_[0]; }
  const Accum&   accum()  const { return *accum_; }

  // Store-path monitor taps for testbench-only checkers.
  auto& storeSpadReadReqVal() { return st_read_ctrl_->dmawrite_spad[0]; }
  auto& storeSpadReadReqRdy() { return spad_->read_req_rdy_bnk[0]; }
  auto& storeSpadReadReqBits() { return st_read_ctrl_->spad_req_bits[0]; }
  auto& storeNormEnqVal() { return st_read_ctrl_->read_req_fire; }
  auto& storeNormEnqRdy() { return write_norm_queue_->enq_rdy; }
  auto& storeNormEnqBits() { return write_dispatch_queue_->deq_bits; }
  auto& storeDmaWriterReqVal() { return dma_writer_->req_val; }
  auto& storeDmaWriterReqRdy() { return dma_writer_->req_rdy; }
  auto& storeDmaWriterReqBits() { return dma_writer_->req_bits; }

  void update();
  void reset();

 private:
  SmeshCmdQueue*           cmd_queue_ = nullptr;
  SmeshUnrolledCmdQueue*   unrolled_cmd_queue_ = nullptr;
  SmeshRS*                 rs_ = nullptr;
  LdCtrl*                  ld_ctrl_ = nullptr;
  DmaReadIssueQueue*       read_issue_queue_ = nullptr;
  ExCtrl*                  ex_ctrl_ = nullptr;
  StCtrl*                  st_ctrl_ = nullptr;
  DmaWriteDispatchQueue*   write_dispatch_queue_ = nullptr;
  StReadCtrl*              st_read_ctrl_ = nullptr;
  std::array<ArbReadSpad*, kSpBanks> arb_read_spad_{};
  std::array<ArbReadAccum*, kAccBanks> arb_read_accum_{};
  std::array<ArbWriteSpad*, kSpBanks> arb_write_spad_{};
  std::array<ArbWriteAccum*, kAccBanks> arb_write_accum_{};
  Output(bit, write_arb_zero_val_);
  Output(DmaReadResp, write_arb_zero_bits_);
  WriteCtrl*               write_ctrl_ = nullptr;
  std::array<ArbRespSpad*, kSpBanks> arb_resp_spad_{};
  DmaWriteNormQueue*       write_norm_queue_ = nullptr;
  StNormCtrl*              st_norm_ctrl_ = nullptr;
  Normalizer*              normalizer_ = nullptr;
  AccScaleUnit*            acc_scale_unit_ = nullptr;
  StScaleCtrl*             st_scale_ctrl_ = nullptr;
  DmaWriteScaleQueue*      write_scale_queue_ = nullptr;
  DmaWriteIssueQueue*      write_issue_queue_ = nullptr;
  StIssueCtrl*             st_issue_ctrl_ = nullptr;
  StIssueMux*              st_issue_mux_ = nullptr;
  DmaWriter*               dma_writer_ = nullptr;
  SpadWriter*              spad_writer_ = nullptr;
  DmaReader*               dma_reader_ = nullptr;
  MvinScaleSplit*          mvin_scale_split_ = nullptr;
  MvinScale*               mvin_scale_ = nullptr;
  MvinScaleAcc*            mvin_scale_acc_ = nullptr;
  Output(bit, mvin_scale_acc_data_rdy_zero_);
  MvinPixelRepeater*       pixel_repeater_ = nullptr;
  MvinLocalRouter*         local_router_ = nullptr;
  Output(bit, local_router_dmaread_spad_rdy_zero_);
  Output(bit, local_router_dmaread_accum_rdy_zero_);
  Spad*                    spad_ = nullptr;
  std::array<SpadDmaReadPipe*, kSpBanks> spad_dma_read_pipe_{};
  std::array<SpadExReadPipe*, kSpBanks> spad_ex_read_pipe_{};
  Accum*                   accum_ = nullptr;
  DmaReadCompletionMux*    completion_mux_ = nullptr;
};

} // namespace smesh
