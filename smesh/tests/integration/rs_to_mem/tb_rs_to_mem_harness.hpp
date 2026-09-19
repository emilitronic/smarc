// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_harness.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
/*
Reusable rs_to_mem (ExCtrl slice) simulation environment: the test-only
glue components and the RsMemHarnessInstance that owns and wires the real
composition around them. See README.md for what's real vs. test-only here.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "Accum.hpp"
#include "ArbComplete.hpp"
#include "ArbReadLocal.hpp"
#include "ArbWriteLocal.hpp"
#include "ExCtrl.hpp"
#include "SmeshCmdQueues.hpp"
#include "SmeshRS.hpp"
#include "Spad.hpp"
#include "SpadReadPipes.hpp"
#include "tb_rs_to_mem_test_cases.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace smesh {
namespace tb {

// Issues one raw SmeshCmd per cycle into cmd_valid/cmd_bits, holding until
// cmd_ready. Waits for spad preload to finish first so ExCtrl never reads
// stale/uninitialized scratchpad data.
class RsMemCmdDriver : public Component {
  DECLARE_COMPONENT(RsMemCmdDriver);

 public:
  RsMemCmdDriver(const std::vector<SmeshCmd>& program, std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, cmd_valid);
  Output(SmeshCmd, cmd_bits);
  Input(bit, cmd_ready);
  Input(bit, preload_done);

  bool done() const { return next_ >= program_.size(); }

  void update();
  void reset();

 private:
  const std::vector<SmeshCmd>& program_;
  std::size_t next_ = 0;
};

// Sequentially writes the test's initial spad image into the real Spad,
// one row (one bank) per cycle, via the same dmaread-shaped write channel
// WriteCtrl would normally drive. Deliberately one write at a time so
// preload itself never exercises Spad's known single-bank-per-cycle limit.
// For when you don't want to rely on LdCtrl to actually do the preload.
class SpadPreloadDriver : public Component {
  DECLARE_COMPONENT(SpadPreloadDriver);

 public:
  SpadPreloadDriver(const std::vector<SpadPreloadRow>& rows, std::string name, COMPONENT_CTOR);

  Clock(clk);
  OutputArray(bit, dmaread_val, kSpBanks);
  OutputArray(DmaReadResp, dmaread_bits, kSpBanks);
  InputArray(bit, dmaread_rdy, kSpBanks);
  Output(bit, done);

  void update();
  void reset();

 private:
  const std::vector<SpadPreloadRow>& rows_;
  std::size_t next_ = 0;
};

// Passively records the sequence of ExCtrl completion tags for the
// checker. Real forwarding into SmeshRS.completed is done by the real
// ArbExLdStComplete -- this component does not drive anything, it only
// taps the same ex_ctrl.completed_val/bits that feeds it.
class CompletionObserver : public Component {
  DECLARE_COMPONENT(CompletionObserver);

 public:
  CompletionObserver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, completed_val);
  Input(SmeshRsTag, completed_bits);

  const std::vector<SmeshRsTag>& observed() const { return observed_; }

  void update();
  void reset();

 private:
  std::vector<SmeshRsTag> observed_;
};

// Converts between ExCtrl's bank-local scratchpad/accumulator ports and
// the legacy full-address structs the shared arbiters still expect.
// Read-side mirrors SmeshTop::updateExCtrlReadReqAdapters() (private
// there); the write-side has no counterpart anywhere -- SmeshTop currently
// ties ExCtrl's write ports to a permanent-zero stub instead of wiring
// them to the arbiters at all, so this is the first place ExCtrl's
// writeback path is connected end-to-end to real Spad/Accum. Both sides
// bridge two conventions that already coexist in the real codebase; see
// README.md.
class ExCtrlMemAdapter : public Component {
  DECLARE_COMPONENT(ExCtrlMemAdapter);

 public:
  ExCtrlMemAdapter(std::string name, COMPONENT_CTOR);

  Clock(clk);

  InputArray(SpadBankReadReq, spad_read_req_bits, kSpBanks);
  OutputArray(SpadReadReq, spad_read_req_legacy_bits, kSpBanks);

  InputArray(bit, spad_write_val, kSpBanks);
  InputArray(SpadBankWriteReq, spad_write_bits, kSpBanks);
  OutputArray(bit, spad_exwrite_val, kSpBanks);
  OutputArray(DmaReadResp, spad_exwrite_bits, kSpBanks);

  InputArray(bit, accum_write_val, kAccBanks);
  InputArray(AccumBankWriteReq, accum_write_bits, kAccBanks);
  OutputArray(bit, accum_exwrite_val, kAccBanks);
  OutputArray(DmaReadResp, accum_exwrite_bits, kAccBanks);

  void updateReadAdapter();
  void updateWriteAdapter();
};

// Constant-signal source for ports this test permanently ties off (the
// store-path read side, the zero-fill/full-width write sources, and
// ExCtrl's accumulator read side, none of which basic/mul_pre exercise).
class TieOff : public Component {
  DECLARE_COMPONENT(TieOff);

 public:
  TieOff(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, zero_bit);
  Output(bit, one_bit);
  Output(SpadReadReq, spad_read_req_zero);
  Output(DmaReadResp, dma_read_resp_zero);
  Output(ExCtrlAccumReadResp, accum_read_resp_zero);
  Output(AccumReadReq, accum_read_req_zero);

  void update();
};

// Owns and wires one independent rs_to_mem (ExCtrl slice) simulation:
// the real command-queue front end, real SmeshRS, ExCtrl (treated as
// opaque), real Spad/Accum and their arbiters, and the small glue above.
class RsMemHarnessInstance {
 public:
  RsMemHarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk);
  ~RsMemHarnessInstance();

  bool activityComplete() const;
  bool passed() const;
  void report() const;
  void debugDumpResponse(const char* label, std::size_t bank) const;  // TEMPORARY diagnostic

 private:
  const RsMemTestCase& test_;

  std::unique_ptr<RsMemCmdDriver> cmd_driver_;
  std::unique_ptr<SpadPreloadDriver> spad_preload_;
  std::unique_ptr<SmeshCmdQueue> cmd_queue_;
  std::unique_ptr<SmeshUnrolledCmdQueue> unrolled_queue_;
  std::unique_ptr<SmeshRS> rs_;
  std::unique_ptr<ExCtrl> ex_ctrl_;
  std::unique_ptr<ArbExLdStComplete> arb_complete_;
  std::unique_ptr<CompletionObserver> completion_observer_;
  std::unique_ptr<ExCtrlMemAdapter> mem_adapter_;
  std::unique_ptr<TieOff> tie_;
  std::unique_ptr<Spad> spad_;
  std::unique_ptr<Accum> accum_;

  std::array<std::unique_ptr<ArbReadSpad>, kSpBanks> arb_read_spad_;
  std::array<std::unique_ptr<ArbWriteSpad>, kSpBanks> arb_write_spad_;
  std::array<std::unique_ptr<ArbRespSpad>, kSpBanks> arb_resp_spad_;
  std::array<std::unique_ptr<SpadDmaReadPipe>, kSpBanks> spad_dma_pipe_;
  std::array<std::unique_ptr<SpadExReadPipe>, kSpBanks> spad_ex_pipe_;
  std::array<std::unique_ptr<ArbWriteAccum>, kAccBanks> arb_write_accum_;
};

} // namespace tb
} // namespace smesh
