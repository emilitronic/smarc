// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_monitor.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 22 2026
#pragma once

#include "Smesh.hpp"

#include <vector>

namespace smesh {
namespace tb {

// Records completed RS tags for the integration test's checker.
class CompletionMonitor {
 public:
  const std::vector<SmeshRsTag>& observed() const { return observed_; }

  void sample(Smesh& top);
  void reset();

 private:
  std::vector<SmeshRsTag> observed_;
};

// Records the generated execution commands on their real output handshake.
class ConvExecuteMonitor {
 public:
  void sample(Smesh& top);
  void reset();
  const std::vector<SmeshCmd>& observed() const { return observed_; }
 private:
  std::vector<SmeshCmd> observed_;
};

// Records accepted local-memory reads, concurrent banks, and Accum response stalls.
class LocalMemMonitor {
 public:
  void sample(const Spad& spad, const Accum& accum);
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
  explicit RsMemMonitor(Smesh& top) : top_(top) {}

  void reset(); // Clear the records before a simulation run or after Sim::reset().
  void sample(); // Call once after each Sim::run() to record that cycle's events.

  // Give test checks read access to the records owned by each monitor.
  const CompletionMonitor& completion() const { return completion_monitor_; }
  const ConvExecuteMonitor& convExecute() const { return conv_execute_monitor_; }
  const LocalMemMonitor& localMem() const { return local_mem_monitor_; }

 private:
  Smesh& top_;
  CompletionMonitor  completion_monitor_;
  ConvExecuteMonitor conv_execute_monitor_;
  LocalMemMonitor    local_mem_monitor_;
};

} // namespace tb
} // namespace smesh
