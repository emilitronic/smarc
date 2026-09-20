// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_harness.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026
/*
Full-Smesh integration harness for raw RS commands and the real top-level
composition. Test setup seeds the behavioral Spad image before the first
simulated cycle; these scenarios do not issue load commands.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "Smesh.hpp"
#include "smem/Dram.hpp"
#include "smem/MemCtrl.hpp"
#include "tb_rs_to_mem_test_cases.hpp"

#include <memory>
#include <string>
#include <vector>

namespace smesh {
namespace tb {

// Sends the test's raw command sequence into Smesh and holds each until accepted.
class RsMemCmdDriver : public Component {
  DECLARE_COMPONENT(RsMemCmdDriver);

 public:
  RsMemCmdDriver(const std::vector<SmeshCmd>& program, std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, cmd_valid);
  Output(SmeshCmd, cmd_bits);
  Input(bit, cmd_ready);

  bool done() const { return next_ >= program_.size(); }

  void update();
  void reset();

 private:
  const std::vector<SmeshCmd>& program_;
  std::size_t next_ = 0;
};

// Records completed RS tags for the integration test's checker.
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

// Owns and wires one full-Smesh simulation for a single scenario.
class RsMemHarnessInstance {
 public:
  RsMemHarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk);
  ~RsMemHarnessInstance();

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
  std::unique_ptr<CompletionObserver> completion_observer_;
  std::unique_ptr<Smesh> top_;
  std::unique_ptr<smem::MemCtrl> mem_;
  std::unique_ptr<smem::Dram> dram_;

  std::size_t max_concurrent_spad_banks_ = 0;
};

} // namespace tb
} // namespace smesh
