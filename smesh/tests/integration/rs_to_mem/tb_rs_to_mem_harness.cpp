// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_harness.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026

#include "tb_rs_to_mem_harness.hpp"

#include <algorithm>
#include <cstdio>

namespace smesh {
namespace tb {

// ********************************************************
// RsMemCmdDriver
// ********************************************************

RsMemCmdDriver::RsMemCmdDriver(const std::vector<SmeshCmd>& program, std::string /*name*/, IMPL_CTOR)
    : program_(program) {
  UPDATE(update).reads(cmd_ready, preload_done).writes(cmd_valid, cmd_bits);
}

void RsMemCmdDriver::update() {
  cmd_valid = 0;
  cmd_bits = SmeshCmd{};
  if (Sim::state == Sim::SimResetting) {
    return;
  }
  if (preload_done == 0 || done()) {
    return;
  }
  cmd_bits = program_[next_];
  cmd_valid = 1;
  if (cmd_ready != 0) {
    ++next_;
  }
}

void RsMemCmdDriver::reset() { next_ = 0; }

// ********************************************************
// SpadPreloadDriver
// ********************************************************

SpadPreloadDriver::SpadPreloadDriver(const std::vector<SpadPreloadRow>& rows, std::string /*name*/, IMPL_CTOR)
    : rows_(rows) {
  next_q_ <= next_d_;
  UPDATE(updateView).reads(next_q_).writes(dmaread_val, dmaread_bits, done);
  UPDATE(updateNextState).reads(next_q_, dmaread_val, dmaread_rdy).writes(next_d_);
}

void SpadPreloadDriver::updateView() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    dmaread_val[bank] = 0;
    dmaread_bits[bank] = DmaReadResp{};
  }
  const auto next = static_cast<std::size_t>(*next_q_);
  if (next >= rows_.size()) {
    done = 1;
    return;
  }
  done = 0;

  const auto& entry = rows_[next];
  const auto bank = entry.laddr.sp_bank();
  DmaReadResp resp{};
  resp.laddr = entry.laddr;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    resp.data[lane] = static_cast<std::uint8_t>(entry.data[lane]);
  }
  resp.mask = static_cast<std::uint8_t>((1u << kDim) - 1u);
  resp.last = false;
  dmaread_val[bank] = 1;
  dmaread_bits[bank] = resp;
}

void SpadPreloadDriver::updateNextState() {
  auto next = static_cast<std::uint32_t>(*next_q_);
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (dmaread_val[bank] != 0 && dmaread_rdy[bank] != 0) {
      ++next;
      break;
    }
  }
  next_d_ = next;
}

void SpadPreloadDriver::reset() {
  next_d_.reset(0);
  done.reset(0);
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    dmaread_val[bank].reset(0);
    dmaread_bits[bank].reset(DmaReadResp{});
  }
}

// ********************************************************
// CompletionObserver
// ********************************************************

CompletionObserver::CompletionObserver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(completed_val, completed_bits);
}

void CompletionObserver::update() {
  if (completed_val != 0) {
    observed_.push_back(*completed_bits);
  }
}

void CompletionObserver::reset() { observed_.clear(); }

// ********************************************************
// ExCtrlMemAdapter
// ********************************************************

ExCtrlMemAdapter::ExCtrlMemAdapter(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateWriteAdapter)
      .reads(spad_write_val, spad_write_bits, accum_write_val, accum_write_bits)
      .writes(spad_exwrite_val, spad_exwrite_bits, accum_exwrite_val, accum_exwrite_bits);
}

void ExCtrlMemAdapter::updateWriteAdapter() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_exwrite_val[bank] = spad_write_val[bank];
    if (spad_write_val[bank] == 0) {
      spad_exwrite_bits[bank] = DmaReadResp{};
      continue;
    }
    const auto req = *spad_write_bits[bank];
    DmaReadResp resp{};
    resp.laddr = makeSpAddr(static_cast<std::uint32_t>(bank * kSpBankRows) +
                            (req.addr & kSpBankRowMask));
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      resp.data[lane] = static_cast<std::uint8_t>(req.data[lane]);
    }
    resp.mask = static_cast<std::uint8_t>(req.mask & 0xffu);
    resp.last = false;
    spad_exwrite_bits[bank] = resp;
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_exwrite_val[bank] = accum_write_val[bank];
    if (accum_write_val[bank] == 0) {
      accum_exwrite_bits[bank] = DmaReadResp{};
      continue;
    }
    const auto req = *accum_write_bits[bank];
    DmaReadResp resp{};
    resp.laddr = makeAccAddr(static_cast<std::uint32_t>(bank * kAccBankRows) +
                             (req.addr & kAccBankRowMask),
                             /*do_accumulate=*/req.acc != 0);
    resp.has_acc_bitwidth = true;
    for (std::size_t lane = 0; lane < kDim; ++lane) {
      const auto value = static_cast<std::uint32_t>(req.data[lane]);
      for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
        resp.data[lane * sizeof(Acc) + byte] =
            static_cast<std::uint8_t>((value >> (8 * byte)) & 0xffu);
      }
    }
    resp.mask = static_cast<std::uint8_t>(req.mask & 0xffu);
    resp.last = false;
    accum_exwrite_bits[bank] = resp;
  }
}

// ********************************************************
// TieOff
// ********************************************************

TieOff::TieOff(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateConstants)
      .writes(zero_bit, one_bit, spad_read_req_zero, dma_read_resp_zero,
              accum_read_resp_zero, accum_read_req_zero);
  UPDATE(updateCompletionDrain).reads(spad_completion);
}

void TieOff::updateConstants() {
  zero_bit = 0;
  one_bit = 1;
  spad_read_req_zero = SpadBankReadReq{};
  dma_read_resp_zero = DmaReadResp{};
  accum_read_resp_zero = ExCtrlAccumReadResp{};
  accum_read_req_zero = AccumReadReq{};
}

void TieOff::updateCompletionDrain() {
  if (!spad_completion.empty()) {
    spad_completion.pop();
  }
}

// ********************************************************
// RsMemHarnessInstance
// ********************************************************

RsMemHarnessInstance::RsMemHarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk)
    : test_(test) {
  cmd_driver_ = std::make_unique<RsMemCmdDriver>(test_.program, prefix + "CmdDriver");
  spad_preload_ = std::make_unique<SpadPreloadDriver>(test_.spad_rows, prefix + "SpadPreload");
  cmd_queue_ = std::make_unique<SmeshCmdQueue>(prefix + "CmdQueue");
  unrolled_queue_ = std::make_unique<SmeshUnrolledCmdQueue>(prefix + "UnrolledCmdQueue");
  rs_ = std::make_unique<SmeshRS>(prefix + "RS");
  ex_ctrl_ = std::make_unique<ExCtrl>(prefix + "ExCtrl");
  arb_complete_ = std::make_unique<ArbExLdStComplete>(prefix + "ArbComplete");
  completion_observer_ = std::make_unique<CompletionObserver>(prefix + "CompletionObserver");
  mem_adapter_ = std::make_unique<ExCtrlMemAdapter>(prefix + "MemAdapter");
  tie_ = std::make_unique<TieOff>(prefix + "TieOff");
  spad_ = std::make_unique<Spad>(prefix + "Spad");
  accum_ = std::make_unique<Accum>(prefix + "Accum");

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad_[bank] = std::make_unique<ArbReadSpad>(prefix + "ArbReadSpad");
    arb_write_spad_[bank] = std::make_unique<ArbWriteSpad>(prefix + "ArbWriteSpad");
    arb_resp_spad_[bank] = std::make_unique<ArbRespSpad>(prefix + "ArbRespSpad");
    spad_dma_pipe_[bank] = std::make_unique<SpadDmaReadPipe>(prefix + "SpadDmaReadPipe");
    spad_ex_pipe_[bank] = std::make_unique<SpadExReadPipe>(prefix + "SpadExReadPipe");
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    arb_write_accum_[bank] = std::make_unique<ArbWriteAccum>(prefix + "ArbWriteAccum");
  }

  // ----- Wire: host command front door -----
  cmd_queue_->cmd_valid << cmd_driver_->cmd_valid;
  cmd_queue_->cmd_bits << cmd_driver_->cmd_bits;
  cmd_driver_->cmd_ready << cmd_queue_->cmd_ready;
  cmd_driver_->preload_done << spad_preload_->done;

  unrolled_queue_->cmd_in << cmd_queue_->cmd_out;
  rs_->alloc_in << unrolled_queue_->cmd_out;
  rs_->issue_ld.sendToBitBucket();
  rs_->issue_st.sendToBitBucket();
  rs_->setExecuteIssuePortEnabled(true);

  // ----- Wire: RS <-> ExCtrl <-> completion loop -----
  // The real completion arbiter, used exactly as SmeshTop uses it, with
  // its unused Load/Store inputs tied off via the real Cascade primitive
  // for "this FIFO is never written, it will always be empty."
  ex_ctrl_->cmd_in << rs_->issue_ex;
  arb_complete_->ex_completed_val << ex_ctrl_->completed_val;
  arb_complete_->ex_completed_bits << ex_ctrl_->completed_bits;
  arb_complete_->ld_completed.wireToZero();
  arb_complete_->st_completed.wireToZero();
  rs_->completed << arb_complete_->rs_completed;
  completion_observer_->completed_val << ex_ctrl_->completed_val;
  completion_observer_->completed_bits << ex_ctrl_->completed_bits;

  // ----- Wire: ExCtrl accumulator reads, permanently tied off (never
  // exercised by basic/mul_pre -- see TieOff's class comment). -----
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    ex_ctrl_->accum_read_req_rdy[bank] << tie_->one_bit;
    ex_ctrl_->accum_read_resp_val[bank] << tie_->zero_bit;
    ex_ctrl_->accum_read_resp_bits[bank] << tie_->accum_read_resp_zero;
  }

  // ----- Wire: ExCtrl spad read path -----
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad_[bank]->exread_val << ex_ctrl_->spad_read_req_val[bank];
    arb_read_spad_[bank]->exread_bits << ex_ctrl_->spad_read_req_bits[bank];
    ex_ctrl_->spad_read_req_rdy[bank] << arb_read_spad_[bank]->exread_rdy;
    arb_read_spad_[bank]->dmawrite_val << tie_->zero_bit;
    arb_read_spad_[bank]->dmawrite_bits << tie_->spad_read_req_zero;
    arb_read_spad_[bank]->read_req_rdy << spad_->read_req_rdy_bnk[bank];
    spad_->read_req_val_bnk[bank] << arb_read_spad_[bank]->read_req_val;
    spad_->read_req_bits_bnk[bank] << arb_read_spad_[bank]->read_req_bits;

    spad_dma_pipe_[bank]->resp_val << spad_->read_resp_val_bnk[bank];
    spad_dma_pipe_[bank]->resp_bits << spad_->read_resp_bits_bnk[bank];
    spad_dma_pipe_[bank]->out_rdy << tie_->one_bit;
    spad_ex_pipe_[bank]->resp_val << spad_->read_resp_val_bnk[bank];
    spad_ex_pipe_[bank]->resp_bits << spad_->read_resp_bits_bnk[bank];
    ex_ctrl_->spad_read_resp_val[bank] << spad_ex_pipe_[bank]->out_val;
    ex_ctrl_->spad_read_resp_bits[bank] << spad_ex_pipe_[bank]->out_bits;
    spad_ex_pipe_[bank]->out_rdy << ex_ctrl_->spad_read_resp_rdy[bank];
    arb_resp_spad_[bank]->read_resp_val << spad_->read_resp_val_bnk[bank];
    arb_resp_spad_[bank]->read_resp_bits << spad_->read_resp_bits_bnk[bank];
    arb_resp_spad_[bank]->dma_resp_rdy << spad_dma_pipe_[bank]->resp_rdy;
    arb_resp_spad_[bank]->ex_resp_rdy << spad_ex_pipe_[bank]->resp_rdy;
    spad_->read_resp_rdy_bnk[bank] << arb_resp_spad_[bank]->read_resp_rdy;
  }

  // ----- Wire: spad write path (preload channel + real ExCtrl writeback) -----
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    mem_adapter_->spad_write_val[bank] << ex_ctrl_->spad_write_val[bank];
    mem_adapter_->spad_write_bits[bank] << ex_ctrl_->spad_write_bits[bank];

    arb_write_spad_[bank]->exwrite_val << mem_adapter_->spad_exwrite_val[bank];
    arb_write_spad_[bank]->exwrite_bits << mem_adapter_->spad_exwrite_bits[bank];
    ex_ctrl_->spad_write_rdy[bank] << arb_write_spad_[bank]->exwrite_rdy;
    arb_write_spad_[bank]->dmaread_val << spad_preload_->dmaread_val[bank];
    arb_write_spad_[bank]->dmaread_bits << spad_preload_->dmaread_bits[bank];
    spad_preload_->dmaread_rdy[bank] << arb_write_spad_[bank]->dmaread_rdy;
    arb_write_spad_[bank]->zerowrite_val << tie_->zero_bit;
    arb_write_spad_[bank]->zerowrite_bits << tie_->dma_read_resp_zero;
    arb_write_spad_[bank]->write_rdy << spad_->write_rdy_bnk[bank];
    spad_->write_val_bnk[bank] << arb_write_spad_[bank]->write_val;
    spad_->write_bits_bnk[bank] << arb_write_spad_[bank]->write_bits;
  }
  tie_->spad_completion << spad_->dma_resp;

  // ----- Wire: accum write path (real ExCtrl writeback only) -----
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    mem_adapter_->accum_write_val[bank] << ex_ctrl_->accum_write_val[bank];
    mem_adapter_->accum_write_bits[bank] << ex_ctrl_->accum_write_bits[bank];

    arb_write_accum_[bank]->exwrite_val << mem_adapter_->accum_exwrite_val[bank];
    arb_write_accum_[bank]->exwrite_bits << mem_adapter_->accum_exwrite_bits[bank];
    ex_ctrl_->accum_write_rdy[bank] << arb_write_accum_[bank]->exwrite_rdy;
    arb_write_accum_[bank]->dmaread_val << tie_->zero_bit;
    arb_write_accum_[bank]->dmaread_bits << tie_->dma_read_resp_zero;
    arb_write_accum_[bank]->dmaread_full_val << tie_->zero_bit;
    arb_write_accum_[bank]->dmaread_full_bits << tie_->dma_read_resp_zero;
    arb_write_accum_[bank]->zerowrite_val << tie_->zero_bit;
    arb_write_accum_[bank]->zerowrite_bits << tie_->dma_read_resp_zero;
    arb_write_accum_[bank]->write_rdy << accum_->write_rdy_bnk[bank];
    accum_->write_val_bnk[bank] << arb_write_accum_[bank]->write_val;
    accum_->write_bits_bnk[bank] << arb_write_accum_[bank]->write_bits;

    accum_->read_req_val_bnk[bank] << tie_->zero_bit;
    accum_->read_req_bits_bnk[bank] << tie_->accum_read_req_zero;
    accum_->read_resp_rdy_bnk[bank] << tie_->one_bit;
  }
  accum_->dma_resp.sendToBitBucket();

  // ----- Clock -----
  cmd_driver_->clk << clk;
  spad_preload_->clk << clk;
  cmd_queue_->clk << clk;
  unrolled_queue_->clk << clk;
  rs_->clk << clk;
  ex_ctrl_->clk << clk;
  arb_complete_->clk << clk;
  completion_observer_->clk << clk;
  mem_adapter_->clk << clk;
  tie_->clk << clk;
  spad_->clk << clk;
  accum_->clk << clk;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    arb_read_spad_[bank]->clk << clk;
    arb_write_spad_[bank]->clk << clk;
    arb_resp_spad_[bank]->clk << clk;
    spad_dma_pipe_[bank]->clk << clk;
    spad_ex_pipe_[bank]->clk << clk;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    arb_write_accum_[bank]->clk << clk;
  }
}

RsMemHarnessInstance::~RsMemHarnessInstance() = default;

bool RsMemHarnessInstance::activityComplete() const {
  return cmd_driver_->done() && rs_->empty();
}

bool RsMemHarnessInstance::passed() const {
  auto expected_tags = test_.expected_completion_tags;
  auto observed_tags = completion_observer_->observed();
  std::sort(expected_tags.begin(), expected_tags.end());
  std::sort(observed_tags.begin(), observed_tags.end());
  if (observed_tags != expected_tags) {
    return false;
  }
  for (const auto& expected : test_.expected_results) {
    for (std::size_t r = 0; r < expected.rows.size(); ++r) {
      const auto addr = expected.base + static_cast<std::uint32_t>(r);
      const auto& actual = accum_->row(addr);
      for (std::size_t c = 0; c < kDim; ++c) {
        if (actual[c] != expected.rows[r][c]) {
          return false;
        }
      }
    }
  }
  return true;
}

void RsMemHarnessInstance::report() const {
  std::printf("  rs_empty=%u cmd_driver_done=%u\n",
              rs_->empty() ? 1u : 0u, cmd_driver_->done() ? 1u : 0u);
  std::printf("  expected completions:");
  for (const auto tag : test_.expected_completion_tags) {
    std::printf(" %u", static_cast<unsigned>(tag));
  }
  std::printf("\n  observed completions:");
  for (const auto tag : completion_observer_->observed()) {
    std::printf(" %u", static_cast<unsigned>(tag));
  }
  std::printf("\n");
  for (const auto& expected : test_.expected_results) {
    for (std::size_t r = 0; r < expected.rows.size(); ++r) {
      const auto addr = expected.base + static_cast<std::uint32_t>(r);
      const auto& actual = accum_->row(addr);
      std::printf("  accum[%u] expected={%d,%d,%d,%d} actual={%d,%d,%d,%d}\n",
                  addr.data(),
                  static_cast<int>(expected.rows[r][0]), static_cast<int>(expected.rows[r][1]),
                  static_cast<int>(expected.rows[r][2]), static_cast<int>(expected.rows[r][3]),
                  static_cast<int>(actual[0]), static_cast<int>(actual[1]),
                  static_cast<int>(actual[2]), static_cast<int>(actual[3]));
    }
  }
}

} // namespace tb
} // namespace smesh
