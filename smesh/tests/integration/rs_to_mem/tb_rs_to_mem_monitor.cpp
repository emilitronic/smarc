// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_monitor.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 22 2026
#include "tb_rs_to_mem_monitor.hpp"

#include <algorithm>

namespace smesh {
namespace tb {

void CompletionMonitor::sample(Smesh& top) {
  if (top.exCtrlCompletedVal() == 1) {
    observed_.push_back(*top.exCtrlCompletedBits());
  }
  if (top.ldCtrlCompletedVal() == 1 && top.ldCtrlCompletedRdy() == 1) {
    observed_.push_back(*top.ldCtrlCompletedBits());
  }
  if (top.stCtrlCompletedVal() == 1 && top.stCtrlCompletedRdy() == 1) {
    observed_.push_back(*top.stCtrlCompletedBits());
  }
}

void CompletionMonitor::reset() { observed_.clear(); }

TraceKey(rs_mem_conv_execute_);

void ConvExecuteMonitor::sample(Smesh& top) {
  // 1. read signals: check whether a command is accepted from the LoopConv unroller
  if (top.loopConvOutVal() == 0 || top.loopConvOutRdy() == 0 || top.loopConvOutBits()->from_conv_loop == 0) return;
  // 2. recognize event: check the command type
  const auto cmd   = top.loopConvOutBits()->cmd;
  const auto funct = static_cast<SmeshFunct>(static_cast<unsigned>(cmd.funct));
  if (funct != SmeshFunct::Preload && funct != SmeshFunct::ComputeFlip && funct != SmeshFunct::ComputeStay) return;
  // 3. record: save the command (append to monitor's recorded command list which test can later check)
  observed_.push_back(cmd);
  // 4. optionally (when trace channel is enabled) print the command for debugging purposes
  s_trace(rs_mem_conv_execute_, "%12s rs1=0x%016llx rs2=0x%016llx\n",
        funct == SmeshFunct::Preload ? "PRELOAD" :
        funct == SmeshFunct::ComputeFlip ? "COMPUTE_FLIP" : "COMPUTE_STAY",
        static_cast<unsigned long long>(cmd.rs1), static_cast<unsigned long long>(cmd.rs2));
}

void ConvExecuteMonitor::reset() { observed_.clear(); }

// Read local-memory handshakes and update the saved totals, peak, and flag.
void LocalMemMonitor::sample(const Spad& spad, const Accum& accum) {
  std::size_t fired = 0;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (spad.read_req_val_bnk[bank] == 1 && spad.read_req_rdy_bnk[bank] == 1) {
      ++fired;
    }
  }
  max_concurrent_spad_banks_ = std::max(max_concurrent_spad_banks_, fired);

  bool ex_read = false;
  bool store_read = false;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (accum.read_req_val_bnk[bank] == 0 || accum.read_req_rdy_bnk[bank] == 0) continue;
    if (accum.read_req_bits_bnk[bank]->from_dma == 0) {
      ++ex_accum_reads_;
      ex_read = true;
    } else {
      ++store_accum_reads_;
      store_read = true;
    }
  }
  concurrent_accum_reads_ |= ex_read && store_read;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (accum.read_resp_val_bnk[bank] == 1 &&
        accum.read_resp_bits_bnk[bank]->from_dma == 0 &&
        accum.read_resp_rdy_bnk[bank] == 0) {
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

// Read the completed cycle's signals through Smesh's public inspection accessors.
void RsMemMonitor::sample() {
  completion_monitor_.sample(top_);
  conv_execute_monitor_.sample(top_);
  local_mem_monitor_.sample(top_.spad(), top_.accum());
}

void RsMemMonitor::reset() {
  completion_monitor_.reset();
  conv_execute_monitor_.reset();
  local_mem_monitor_.reset();
}

} // namespace tb
} // namespace smesh
