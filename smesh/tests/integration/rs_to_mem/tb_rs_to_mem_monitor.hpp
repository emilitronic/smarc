// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_monitor.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 22 2026
#pragma once

#include <cascade/Cascade.hpp>

#include "Smesh.hpp"

#include <memory>
#include <string>
#include <vector>

namespace smesh {
namespace tb {

// Records completed RS tags for the integration test's checker.
class CompletionMonitor : public Component {
  DECLARE_COMPONENT(CompletionMonitor);

 public:
  CompletionMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit,        completed_val);
  Input(SmeshRsTag, completed_bits);
  Input(bit,        ld_completed_val);
  Input(SmeshRsTag, ld_completed_bits);
  Input(bit,        ld_completed_rdy);
  Input(bit,        st_completed_val);
  Input(SmeshRsTag, st_completed_bits);
  Input(bit,        st_completed_rdy);

  const std::vector<SmeshRsTag>& observed() const { return observed_; }

  void update();
  void reset();

 private:
  std::vector<SmeshRsTag> observed_;
};

// Records the generated execution commands on their real output handshake.
class ConvExecuteMonitor : public Component {
  DECLARE_COMPONENT(ConvExecuteMonitor);
 public:
  ConvExecuteMonitor(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit,            cmd_val);
  Input(bit,            cmd_rdy);
  Input(SmeshQueuedCmd, cmd_bits);
  void update();
  void reset();
  const std::vector<SmeshCmd>& observed() const { return observed_; }
 private:
  std::vector<SmeshCmd> observed_;
};

// Records accepted local-memory reads, concurrent banks, and Accum response stalls.
class LocalMemMonitor : public Component {
  DECLARE_COMPONENT(LocalMemMonitor);

 public:
  LocalMemMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  InputArray(bit,              spad_read_req_val, kSpBanks);
  InputArray(bit,              spad_read_req_rdy, kSpBanks);
  InputArray(bit,              accum_read_req_val, kAccBanks);
  InputArray(bit,              accum_read_req_rdy, kAccBanks);
  InputArray(AccumBankReadReq, accum_read_req_bits, kAccBanks);
  InputArray(bit,              accum_read_resp_val, kAccBanks);
  InputArray(bit,              accum_read_resp_rdy, kAccBanks);
  InputArray(AccumReadResp,    accum_read_resp_bits, kAccBanks);

  void update();
  void reset();

  std::size_t maxConcurrentSpadBanks()     const { return max_concurrent_spad_banks_; }
  std::size_t exAccumReads()               const { return ex_accum_reads_; }
  std::size_t storeAccumReads()            const { return store_accum_reads_; }
  bool        concurrentAccumReads()       const { return concurrent_accum_reads_; }
  std::size_t exAccumResponseStallCycles() const { return ex_accum_response_stall_cycles_; }

 private:
  // Summary values for one simulation run; no per-cycle history is retained.
  std::size_t max_concurrent_spad_banks_      = 0;
  std::size_t ex_accum_reads_                 = 0;
  std::size_t store_accum_reads_              = 0;
  bool        concurrent_accum_reads_         = false;
  std::size_t ex_accum_response_stall_cycles_ = 0;
};

// Collects Smesh signals for test checks: completion tags, LoopConv commands,
// and local-memory activity. The runner owns this monitor separately from
// the harness that builds Smesh and the helper that calculates run timing.
class RsMemMonitor {
 public:
  RsMemMonitor(Smesh& top, const std::string& prefix, Clock& clk);

  // Give test checks read access to the records owned by each monitor.
  const CompletionMonitor& completion() const { return *completion_monitor_; }
  const ConvExecuteMonitor& convExecute() const { return *conv_execute_monitor_; }
  const LocalMemMonitor& localMem() const { return *local_mem_monitor_; }

 private:
  std::unique_ptr<CompletionMonitor>  completion_monitor_;
  std::unique_ptr<ConvExecuteMonitor> conv_execute_monitor_;
  std::unique_ptr<LocalMemMonitor>    local_mem_monitor_;
};

} // namespace tb
} // namespace smesh
