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
class CompletionObserver : public Component {
  DECLARE_COMPONENT(CompletionObserver);

 public:
  CompletionObserver(std::string name, COMPONENT_CTOR);

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
class ConvExecuteObserver : public Component {
  DECLARE_COMPONENT(ConvExecuteObserver);
 public:
  ConvExecuteObserver(std::string name, COMPONENT_CTOR);
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

// Collects Smesh signals for test checks: completion tags, LoopConv commands,
// and local-memory activity. The runner owns this monitor separately from
// the harness that builds Smesh and the helper that calculates run timing.
class RsMemMonitor {
 public:
  RsMemMonitor(Smesh& top, const std::string& prefix, Clock& clk);

  // Call once after each Sim::run() to record Spad/Accum read activity.
  // The two wired observers above record their signals during Sim::run().
  void sampleBankConcurrency();

  const std::vector<SmeshRsTag>& completions() const { return completion_observer_->observed(); }
  const std::vector<SmeshCmd>& convExecuteCommands() const { return conv_execute_observer_->observed(); }
  std::size_t maxConcurrentSpadBanks() const { return max_concurrent_spad_banks_; }
  std::size_t exAccumReads() const { return ex_accum_reads_; }
  std::size_t storeAccumReads() const { return store_accum_reads_; }
  bool concurrentAccumReads() const { return concurrent_accum_reads_; }
  std::size_t exAccumResponseStallCycles() const { return ex_accum_response_stall_cycles_; }

 private:
  Smesh& top_;
  std::unique_ptr<CompletionObserver> completion_observer_;
  std::unique_ptr<ConvExecuteObserver> conv_execute_observer_;

  // Summary values for one simulation run; no per-cycle history is retained.
  std::size_t max_concurrent_spad_banks_      = 0; // max num of spad banks to accept read in a cycle
  std::size_t ex_accum_reads_                 = 0;
  std::size_t store_accum_reads_              = 0;
  bool        concurrent_accum_reads_         = false;
  std::size_t ex_accum_response_stall_cycles_ = 0;
};

} // namespace tb
} // namespace smesh
