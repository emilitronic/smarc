// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_harness.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026
/*
Builds and connects Smesh, its command driver, and simulated external memory.
Initializes memory images and checks results against the selected test case.
Signal sampling and recorded observations live in tb_rs_to_mem_monitor.*.
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

class RsMemMonitor;

// Sends the test's raw command sequence into Smesh and holds each until accepted.
class RsMemCmdDriver : public Component {
  DECLARE_COMPONENT(RsMemCmdDriver);

 public:
  RsMemCmdDriver(const std::vector<SmeshCmd>& program, std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit,      cmd_valid);
  Output(SmeshCmd, cmd_bits);
  Input(bit,       cmd_ready);

  bool done() const { return next_ >= program_.size(); }

  void update();
  void reset();

 private:
  const std::vector<SmeshCmd>& program_;
  std::size_t next_ = 0;
};

// Owns and wires one full-Smesh simulation for a single scenario.
class RsMemHarnessInstance {
 public:
  RsMemHarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk);
  ~RsMemHarnessInstance();

  bool activityComplete() const;
  bool inputCommandAccepted() const;
  bool passed(const RsMemMonitor& monitor) const;
  void report(const RsMemMonitor& monitor) const;
  void initializeMemoryImages();

  // The runner gives the monitor access to the Smesh instance built here.
  Smesh& smesh() { return *top_; }

 private:
  bool dramResultsMatch() const;
  bool convExecuteMatches(const RsMemMonitor& monitor) const;
  const RsMemTestCase& test_;

  std::unique_ptr<RsMemCmdDriver> cmd_driver_;
  std::unique_ptr<Smesh> top_;
  std::unique_ptr<smem::MemCtrl> mem_;
  std::unique_ptr<smem::Dram> dram_;
};

} // namespace tb
} // namespace smesh
