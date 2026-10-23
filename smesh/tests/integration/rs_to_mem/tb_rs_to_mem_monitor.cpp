// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_monitor.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 22 2026
#include "tb_rs_to_mem_monitor.hpp"

#include <algorithm>

namespace smesh {
namespace tb {

CompletionObserver::CompletionObserver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(completed_val, completed_bits,
                       ld_completed_val, ld_completed_bits, ld_completed_rdy,
                       st_completed_val, st_completed_bits, st_completed_rdy);
}

void CompletionObserver::update() {
  if (completed_val == 1) {
    observed_.push_back(*completed_bits);
  }
  if (ld_completed_val == 1 && ld_completed_rdy == 1) {
    observed_.push_back(*ld_completed_bits);
  }
  if (st_completed_val == 1 && st_completed_rdy == 1) {
    observed_.push_back(*st_completed_bits);
  }
}

void CompletionObserver::reset() { observed_.clear(); }

TraceKey(rs_mem_conv_execute_);

ConvExecuteObserver::ConvExecuteObserver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_val, cmd_rdy, cmd_bits);
}

void ConvExecuteObserver::update() {
  if (Sim::state == Sim::SimResetting || cmd_val == 0 || cmd_rdy == 0 ||
      cmd_bits->from_conv_loop == 0) return;
  const auto cmd = cmd_bits->cmd;
  const auto funct = static_cast<SmeshFunct>(static_cast<unsigned>(cmd.funct));
  if (funct != SmeshFunct::Preload && funct != SmeshFunct::ComputeFlip &&
      funct != SmeshFunct::ComputeStay) return;
  observed_.push_back(cmd);
  trace(rs_mem_conv_execute_, "%12s rs1=0x%016llx rs2=0x%016llx\n",
        funct == SmeshFunct::Preload ? "PRELOAD" :
        funct == SmeshFunct::ComputeFlip ? "COMPUTE_FLIP" : "COMPUTE_STAY",
        static_cast<unsigned long long>(cmd.rs1), static_cast<unsigned long long>(cmd.rs2));
}

void ConvExecuteObserver::reset() { observed_.clear(); }

// Connect the completion and LoopConv command recorders before simulation starts.
RsMemMonitor::RsMemMonitor(Smesh& top, const std::string& prefix, Clock& clk)
    : top_(top) {
  completion_observer_   = std::make_unique<CompletionObserver>(prefix + "CompletionObserver");
  conv_execute_observer_ = std::make_unique<ConvExecuteObserver>(prefix + "ConvExecuteObserver");

  completion_observer_->completed_val     << top_.exCtrlCompletedVal();
  completion_observer_->completed_bits    << top_.exCtrlCompletedBits();
  completion_observer_->ld_completed_val  << top_.ldCtrlCompletedVal();
  completion_observer_->ld_completed_bits << top_.ldCtrlCompletedBits();
  completion_observer_->ld_completed_rdy  << top_.ldCtrlCompletedRdy();
  completion_observer_->st_completed_val  << top_.stCtrlCompletedVal();
  completion_observer_->st_completed_bits << top_.stCtrlCompletedBits();
  completion_observer_->st_completed_rdy  << top_.stCtrlCompletedRdy();
  conv_execute_observer_->cmd_val         << top_.loopConvOutVal();
  conv_execute_observer_->cmd_rdy         << top_.loopConvOutRdy();
  conv_execute_observer_->cmd_bits        << top_.loopConvOutBits();

  completion_observer_->clk   << clk;
  conv_execute_observer_->clk << clk;
}

// Read local-memory handshakes and update the saved totals, peak, and flag.
void RsMemMonitor::sampleBankConcurrency() {
  std::size_t fired = 0;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (top_.spad().read_req_val_bnk[bank] != 0 &&
        top_.spad().read_req_rdy_bnk[bank] != 0) {
      ++fired;
    }
  }
  max_concurrent_spad_banks_ = std::max(max_concurrent_spad_banks_, fired);

  bool ex_read = false;
  bool store_read = false;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (top_.accum().read_req_val_bnk[bank] == 0 ||
        top_.accum().read_req_rdy_bnk[bank] == 0) continue;
    if (top_.accum().read_req_bits_bnk[bank]->from_dma == 0) {
      ++ex_accum_reads_;
      ex_read = true;
    } else {
      ++store_accum_reads_;
      store_read = true;
    }
  }
  concurrent_accum_reads_ |= ex_read && store_read;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (top_.accum().read_resp_val_bnk[bank] != 0 &&
        top_.accum().read_resp_bits_bnk[bank]->from_dma == 0 &&
        top_.accum().read_resp_rdy_bnk[bank] == 0) {
      ++ex_accum_response_stall_cycles_;
    }
  }
}

} // namespace tb
} // namespace smesh
