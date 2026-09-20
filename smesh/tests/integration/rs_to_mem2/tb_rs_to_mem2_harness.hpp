// **********************************************************************
// smesh/tests/integration/rs_to_mem2/tb_rs_to_mem2_harness.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026
/*
rs_to_mem2: the same test scenarios as rs_to_mem (reuses its
RsMemTestCase/rsMemTestCases() directly. The cases run through the real
top-level Smesh composition. The harness seeds the behavioral Spad memory
image before the first simulated cycle; this is setup, not a write
transaction. The real Load domain remains idle in these scenarios.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "Smesh.hpp"
#include "smem/Dram.hpp"
#include "smem/MemCtrl.hpp"
#include "tb_rs_to_mem_harness.hpp"
#include "tb_rs_to_mem_test_cases.hpp"

#include <memory>
#include <string>

namespace smesh {
namespace tb {

class RsMem2StartSignal : public Component {
  DECLARE_COMPONENT(RsMem2StartSignal);

 public:
  RsMem2StartSignal(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, ready);
  void update();
  void reset();
};

// Owns and wires one independent rs_to_mem2 simulation.
class RsMem2HarnessInstance {
 public:
  RsMem2HarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk);
  ~RsMem2HarnessInstance();

  bool activityComplete() const;
  bool passed() const;
  void report() const;
  void initializeSpadImage();

  // Call once per cycle, after Sim::run(), from the runner loop -- Smesh's
  // Spad is private, so bank concurrency is sampled from outside via
  // Smesh's own read-only taps rather than a wired monitor component.
  void sampleBankConcurrency();

 private:
  const RsMemTestCase& test_;

  std::unique_ptr<RsMemCmdDriver> cmd_driver_;
  std::unique_ptr<RsMem2StartSignal> start_signal_;
  std::unique_ptr<CompletionObserver> completion_observer_;
  std::unique_ptr<Smesh> top_;
  std::unique_ptr<smem::MemCtrl> mem_;
  std::unique_ptr<smem::Dram> dram_;

  std::size_t max_concurrent_spad_banks_ = 0;
};

} // namespace tb
} // namespace smesh
