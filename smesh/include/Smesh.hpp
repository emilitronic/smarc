// **********************************************************************
// smesh/include/Smesh.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026
/*
Top-level smesh composition point -- a corrected copy of SmeshTop.

Identical to SmeshTop except: ExCtrl's write ports (spad_write_ and
accum_write_, both directions) are wired to the real write arbiters,
matching what smesh/tests/integration/rs_to_mem's test harness has been
doing independently since 2026-09-19. In SmeshTop, these are still
permanently stubbed to zero (forward) and left entirely unconnected
(return path) -- see doc/claude_smesh_notes.md for how that was found.
This file exists as a non-destructive parallel version so SmeshTop and its
existing tb_smesh_top_* tests are untouched; see
smesh/tests/integration/rs_to_mem2/ for the test harness built on this.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include <array>

#include "Accum.hpp"
#include "AccScaleUnit.hpp"
#include "ArbComplete.hpp"
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

class Smesh : public Component {
  DECLARE_COMPONENT(Smesh);

 public:
  Smesh(std::string name, COMPONENT_CTOR);
  ~Smesh() override;

  Clock(clk);

  Input(bit, cmd_valid);
  Input(SmeshCmd, cmd_bits);
  Output(bit, cmd_ready);

  // Memory accessors let the testbench connect the current memory boundary.
  auto& memReq() { return dma_reader_->mem_req; }
  auto& memResp() { return dma_reader_->mem_resp; }

  // narrow inspection accessors for testbench to check internal state
  const SmeshRS& rs()     const { return *rs_; }
  const LdCtrl&  ldCtrl() const { return *ld_ctrl_; }
  const Spad&    spad()   const { return *spad_; }
  const SpadDmaReadPipe& spadDmaReadPipe() const { return *spad_dma_read_pipe_[0]; }
  const Accum&   accum()  const { return *accum_; }

  // Execute-domain completion tap -- SmeshTop doesn't expose this since it
  // never needed to observe ExCtrl completions directly from outside; this
  // harness does (see CompletionObserver in rs_to_mem2's harness).
  auto& exCtrlCompletedVal()  { return ex_ctrl_->completed_val; }
  auto& exCtrlCompletedBits() { return ex_ctrl_->completed_bits; }

  // Per-bank Spad read-accept taps, for a testbench to observe multi-bank
  // concurrency the same way rs_to_mem's own SpadBankConcurrencyMonitor does.
  auto& spadReadReqVal(std::size_t bank) { return spad_->read_req_val_bnk[bank]; }
  auto& spadReadReqRdy(std::size_t bank) { return spad_->read_req_rdy_bnk[bank]; }

  // Test-only Spad preload bypass, same purpose as rs_to_mem's
  // SpadPreloadDriver: writes initial memory contents directly instead of
  // routing through the real Load domain (LdCtrl/DmaReader), which is
  // separately-unproven territory for multiple sequential requests -- see
  // doc/claude_smesh_notes.md. Claims ArbWriteSpad's zerowrite slot, the
  // one write-arbiter input a full Smesh doesn't otherwise need for any
  // current rs_to_mem2 scenario (real zero-fill writes aren't exercised
  // either). A non-test build should tie these to zero, same as SmeshTop
  // does internally.
  InputArray(bit, spad_preload_val, kSpBanks);
  InputArray(DmaReadResp, spad_preload_bits, kSpBanks);
  OutputArray(bit, spad_preload_rdy, kSpBanks);

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
  void updateExWriteAdapter();
  void reset();

 private:
  // Converts ExCtrl's bank-local SpadBankWriteReq/AccumBankWriteReq into the
  // legacy DmaReadResp the write arbiters/memories still expect -- same
  // conversion rs_to_mem's test-only ExCtrlMemAdapter performs, ported here
  // because it's load-bearing for the real write path, not test-only glue.
  OutputArray(bit, ex_spad_write_val_, kSpBanks);
  OutputArray(DmaReadResp, ex_spad_write_bits_, kSpBanks);
  OutputArray(bit, ex_accum_write_val_, kAccBanks);
  OutputArray(DmaReadResp, ex_accum_write_bits_, kAccBanks);

  SmeshCmdQueue*           cmd_queue_ = nullptr;
  SmeshUnrolledCmdQueue*   unrolled_cmd_queue_ = nullptr;
  SmeshRS*                 rs_ = nullptr;
  ArbExLdStComplete*       completion_arb_ = nullptr;
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
  AccumExResp*             accum_ex_resp_ = nullptr;
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
  MvinPixelRepeater*       pixel_repeater_ = nullptr;
  MvinLocalRouter*         local_router_ = nullptr;
  Spad*                    spad_ = nullptr;
  std::array<SpadDmaReadPipe*, kSpBanks> spad_dma_read_pipe_{};
  std::array<SpadExReadPipe*, kSpBanks> spad_ex_read_pipe_{};
  Accum*                   accum_ = nullptr;
  DmaReadCompletionMux*    completion_mux_ = nullptr;
};

} // namespace smesh
