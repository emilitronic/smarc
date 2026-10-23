// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_monitor.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 22 2026
#include "tb_rs_to_mem_monitor.hpp"

#include <algorithm>

namespace smesh {
namespace tb {

CompletionMonitor::CompletionMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(completed_val, completed_bits,
                       ld_completed_val, ld_completed_bits, ld_completed_rdy,
                       st_completed_val, st_completed_bits, st_completed_rdy);
}

void CompletionMonitor::update() {
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

void CompletionMonitor::reset() { observed_.clear(); }

TraceKey(rs_mem_conv_execute_);

ConvExecuteMonitor::ConvExecuteMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_val, cmd_rdy, cmd_bits);
}

void ConvExecuteMonitor::update() {
  // 1. read signals: check whether a command is accepted from the LoopConv unroller
  if (Sim::state == Sim::SimResetting || cmd_val == 0 || cmd_rdy == 0 || cmd_bits->from_conv_loop == 0) return;
  // 2. recognize event: check the command type
  const auto cmd   = cmd_bits->cmd;
  const auto funct = static_cast<SmeshFunct>(static_cast<unsigned>(cmd.funct));
  if (funct != SmeshFunct::Preload && funct != SmeshFunct::ComputeFlip && funct != SmeshFunct::ComputeStay) return;
  // 3. record: save the command (append to monitor's recorded command list which test can later check)
  observed_.push_back(cmd);
  // 4. optionally (when trace channel is enabled) print the command for debugging purposes
  trace(rs_mem_conv_execute_, "%12s rs1=0x%016llx rs2=0x%016llx\n",
        funct == SmeshFunct::Preload ? "PRELOAD" :
        funct == SmeshFunct::ComputeFlip ? "COMPUTE_FLIP" : "COMPUTE_STAY",
        static_cast<unsigned long long>(cmd.rs1), static_cast<unsigned long long>(cmd.rs2));
}

void ConvExecuteMonitor::reset() { observed_.clear(); }

LocalMemMonitor::LocalMemMonitor(std::string /*name*/, IMPL_CTOR) {
  // Sample once per clock cycle, after the watched signals have been computed.
  UPDATE(update).clock(clk)
      .reads(spad_read_req_val, spad_read_req_rdy,
             accum_read_req_val, accum_read_req_rdy, accum_read_req_bits,
             accum_read_resp_val, accum_read_resp_rdy, accum_read_resp_bits);
}

// Read local-memory handshakes and update the saved totals, peak, and flag.
void LocalMemMonitor::update() {
  if (Sim::state == Sim::SimResetting) return;

  std::size_t fired = 0;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (spad_read_req_val[bank] == 1 && spad_read_req_rdy[bank] == 1) {
      ++fired;
    }
  }
  max_concurrent_spad_banks_ = std::max(max_concurrent_spad_banks_, fired);

  bool ex_read = false;
  bool store_read = false;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (accum_read_req_val[bank] == 0 || accum_read_req_rdy[bank] == 0) continue;
    if (accum_read_req_bits[bank]->from_dma == 0) {
      ++ex_accum_reads_;
      ex_read = true;
    } else {
      ++store_accum_reads_;
      store_read = true;
    }
  }
  concurrent_accum_reads_ |= ex_read && store_read;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (accum_read_resp_val[bank] == 1 &&
        accum_read_resp_bits[bank]->from_dma == 0 &&
        accum_read_resp_rdy[bank] == 0) {
      ++ex_accum_response_stall_cycles_;
    }
  }
}

void LocalMemMonitor::reset() {
  max_concurrent_spad_banks_ = 0;
  ex_accum_reads_ = 0;
  store_accum_reads_ = 0;
  concurrent_accum_reads_ = false;
  ex_accum_response_stall_cycles_ = 0;
}

// Connect all three signal monitors before simulation starts.
RsMemMonitor::RsMemMonitor(Smesh& top, const std::string& prefix, Clock& clk) {
  completion_monitor_   = std::make_unique<CompletionMonitor>(prefix + "CompletionMonitor");
  conv_execute_monitor_ = std::make_unique<ConvExecuteMonitor>(prefix + "ConvExecuteMonitor");
  local_mem_monitor_    = std::make_unique<LocalMemMonitor>(prefix + "LocalMemMonitor");

  completion_monitor_->completed_val     << top.exCtrlCompletedVal();
  completion_monitor_->completed_bits    << top.exCtrlCompletedBits();
  completion_monitor_->ld_completed_val  << top.ldCtrlCompletedVal();
  completion_monitor_->ld_completed_bits << top.ldCtrlCompletedBits();
  completion_monitor_->ld_completed_rdy  << top.ldCtrlCompletedRdy();
  completion_monitor_->st_completed_val  << top.stCtrlCompletedVal();
  completion_monitor_->st_completed_bits << top.stCtrlCompletedBits();
  completion_monitor_->st_completed_rdy  << top.stCtrlCompletedRdy();
  conv_execute_monitor_->cmd_val         << top.loopConvOutVal();
  conv_execute_monitor_->cmd_rdy         << top.loopConvOutRdy();
  conv_execute_monitor_->cmd_bits        << top.loopConvOutBits();

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    local_mem_monitor_->spad_read_req_val[bank] << top.spadReadReqVal()[bank];
    local_mem_monitor_->spad_read_req_rdy[bank] << top.spadReadReqRdy()[bank];
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    local_mem_monitor_->accum_read_req_val[bank]   << top.accumReadReqVal()[bank];
    local_mem_monitor_->accum_read_req_rdy[bank]   << top.accumReadReqRdy()[bank];
    local_mem_monitor_->accum_read_req_bits[bank]  << top.accumReadReqBits()[bank];
    local_mem_monitor_->accum_read_resp_val[bank]  << top.accumReadRespVal()[bank];
    local_mem_monitor_->accum_read_resp_rdy[bank]  << top.accumReadRespRdy()[bank];
    local_mem_monitor_->accum_read_resp_bits[bank] << top.accumReadRespBits()[bank];
  }

  completion_monitor_->clk   << clk;
  conv_execute_monitor_->clk << clk;
  local_mem_monitor_->clk    << clk;
}

} // namespace tb
} // namespace smesh
