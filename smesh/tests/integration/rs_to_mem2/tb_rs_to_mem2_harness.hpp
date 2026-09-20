// **********************************************************************
// smesh/tests/integration/rs_to_mem2/tb_rs_to_mem2_harness.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026
/*
rs_to_mem2: the same test scenarios as rs_to_mem (reuses its
RsMemTestCase/rsMemTestCases() *and* its RsMemCmdDriver/SpadPreloadDriver/
CompletionObserver test-only glue directly, see
../rs_to_mem/tb_rs_to_mem_harness.hpp), run through the real top-level
Smesh composition instead of a hand-wired subset of components. Preload
uses the same direct-bypass approach rs_to_mem already uses (via Smesh's
spad_preload_* ports, which claim ArbWriteSpad's otherwise-unused
zerowrite slot) rather than the real Load domain (LdCtrl/DmaReader) --
that path is separately known to hang after 4 sequential real Mvins, an
unrelated, not-yet-understood issue logged in doc/claude_smesh_notes.md,
deliberately not exercised here.
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

// Owns and wires one independent rs_to_mem2 simulation: a real Smesh (its
// memReq/memResp tied to a real, but otherwise unused, Dram+MemCtrl pair
// since Smesh's Load domain expects a memory boundary to exist even when
// not exercised), plus the same command driver/preload/completion glue
// rs_to_mem already uses.
class RsMem2HarnessInstance {
 public:
  RsMem2HarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk);
  ~RsMem2HarnessInstance();

  bool activityComplete() const;
  bool passed() const;
  void report() const;

  // Call once per cycle, after Sim::run(), from the runner loop -- Smesh's
  // Spad is private, so bank concurrency is sampled from outside via
  // Smesh's own read-only taps rather than a wired monitor component.
  void sampleBankConcurrency();

 private:
  const RsMemTestCase& test_;

  std::unique_ptr<RsMemCmdDriver> cmd_driver_;
  std::unique_ptr<SpadPreloadDriver> spad_preload_;
  std::unique_ptr<CompletionObserver> completion_observer_;
  std::unique_ptr<Smesh> top_;
  std::unique_ptr<smem::MemCtrl> mem_;
  std::unique_ptr<smem::Dram> dram_;

  std::size_t max_concurrent_spad_banks_ = 0;
};

} // namespace tb
} // namespace smesh
