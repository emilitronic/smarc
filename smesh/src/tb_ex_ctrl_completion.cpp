// **********************************************************************
// smesh/src/tb_ex_ctrl_completion.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 15 2026
/*
Focused ExCtrlCompletion arbitration and pending-state test.

cmake --build build --target tb_ex_ctrl_completion -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl_completion -trace '*'/completion_tb_
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlCompletion.hpp"

#include <cstdio>
#include <cstdint>

namespace {

constexpr smesh::SmeshRsTag kFirstPendingTag  = 11;
constexpr smesh::SmeshRsTag kSlotZeroTag      = 20;
constexpr smesh::SmeshRsTag kSlotOneTag       = 21;
constexpr smesh::SmeshRsTag kConfigTag        = 30;
constexpr smesh::SmeshRsTag kMeshTag          = 40;
constexpr smesh::SmeshRsTag kReplaceOldTag    = 50;
constexpr smesh::SmeshRsTag kReplaceNewTag    = 51;

struct ExpectedCompletion {
  bit                   val = 0;
  smesh::SmeshRsTag     tag = 0;
  bit                   pending = 0;
  const char*           source = "---";
};

ExpectedCompletion expectedAt(std::uint32_t cycle) {
  switch (cycle) {
    case 2:  return {1, kFirstPendingTag, 1, "P0"};
    case 5:  return {1, kConfigTag,       1, "CFG"};
    case 6:  return {1, kMeshTag,         1, "MSH"};
    case 7:  return {1, kSlotZeroTag,     1, "P0"};
    case 8:  return {1, kSlotOneTag,      1, "P1"};
    case 11: return {1, kReplaceOldTag,   1, "P0"};
    case 12: return {1, kReplaceNewTag,   1, "P0"};
    default: return {};
  }
}

} // namespace

class CompletionDriver : public Component {
  DECLARE_COMPONENT(CompletionDriver);

 public:
  CompletionDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Output(bit, config_val);
  Output(bit, config_rs_tag_val);
  Output(smesh::SmeshRsTag, config_rs_tag);
  OutputArray(bit, pending_completed_set_val, 2);
  OutputArray(smesh::SmeshRsTag, pending_completed_set_bits, 2);
  Output(bit, mesh_completed_rs_tag_fire);
  Output(smesh::SmeshRsTag, mesh_completed_bits);
  Output(u32, cycle);

  void update();
  void reset();

 private:
  Register(u32, cycle_D_);
};

CompletionDriver::CompletionDriver(std::string /*name*/, IMPL_CTOR) {
  cycle <= cycle_D_;

  UPDATE(update)
      .reads(cycle)
      .writes(config_val, config_rs_tag_val, config_rs_tag,
              pending_completed_set_val, pending_completed_set_bits,
              mesh_completed_rs_tag_fire, mesh_completed_bits,
              cycle_D_);
}

void CompletionDriver::update() {
  const auto now = static_cast<std::uint32_t>(*cycle);

  config_val = 0;
  config_rs_tag_val = 0;
  config_rs_tag = 0;
  mesh_completed_rs_tag_fire = 0;
  mesh_completed_bits = 0;
  for (std::size_t i = 0; i < 2; ++i) {
    pending_completed_set_val[i] = 0;
    pending_completed_set_bits[i] = 0;
  }

  switch (now) {
    case 1:
      pending_completed_set_val[0] = 1;
      pending_completed_set_bits[0] = kFirstPendingTag;
      break;
    case 4:
      pending_completed_set_val[0] = 1;
      pending_completed_set_bits[0] = kSlotZeroTag;
      pending_completed_set_val[1] = 1;
      pending_completed_set_bits[1] = kSlotOneTag;
      break;
    case 5:
      config_val = 1;
      config_rs_tag_val = 1;
      config_rs_tag = kConfigTag;
      break;
    case 6:
      mesh_completed_rs_tag_fire = 1;
      mesh_completed_bits = kMeshTag;
      break;
    case 10:
      pending_completed_set_val[0] = 1;
      pending_completed_set_bits[0] = kReplaceOldTag;
      break;
    case 11:
      // A new slot value wins over clearing the old value reported this cycle.
      pending_completed_set_val[0] = 1;
      pending_completed_set_bits[0] = kReplaceNewTag;
      break;
    default:
      break;
  }

  cycle_D_ = now + 1;
}

void CompletionDriver::reset() {
  cycle_D_.reset(0);
  config_val.reset(0);
  config_rs_tag_val.reset(0);
  config_rs_tag.reset(0);
  mesh_completed_rs_tag_fire.reset(0);
  mesh_completed_bits.reset(0);
  for (std::size_t i = 0; i < 2; ++i) {
    pending_completed_set_val[i].reset(0);
    pending_completed_set_bits[i].reset(0);
  }
}

class CompletionMonitor : public Component {
  DECLARE_COMPONENT(CompletionMonitor);

 public:
  CompletionMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(u32, cycle);
  Input(bit, completed_val);
  Input(smesh::SmeshRsTag, completed_bits);
  Input(bit, pending_completed_val);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool done_ = false;
  bool passed_ = true;
};

TraceKey(completion_tb_);

CompletionMonitor::CompletionMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cycle, completed_val, completed_bits, pending_completed_val);
}

void CompletionMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  const auto now = static_cast<std::uint32_t>(*cycle);
  const auto expected = expectedAt(now);
  const auto actual_tag = static_cast<smesh::SmeshRsTag>(*completed_bits);
  const bool matches = completed_val == expected.val &&
                       actual_tag == expected.tag &&
                       pending_completed_val == expected.pending;

  s_trace(completion_tb_,
          "cycle=%02u src=%s pending=%u completed{v=%u tag=%03u} %s\n",
          static_cast<unsigned>(now),
          expected.source,
          static_cast<unsigned>(pending_completed_val != 0),
          static_cast<unsigned>(completed_val != 0),
          static_cast<unsigned>(actual_tag),
          matches ? "OK" : "ERROR");

  passed_ = passed_ && matches;
  done_ = now >= 13;
}

void CompletionMonitor::reset() {
  done_ = false;
  passed_ = true;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  CompletionDriver driver("Driver");
  smesh::ExCtrlCompletion completion("Completion");
  CompletionMonitor monitor("Monitor");

  completion.config_val << driver.config_val;
  completion.config_rs_tag_val << driver.config_rs_tag_val;
  completion.config_rs_tag << driver.config_rs_tag;
  completion.mesh_completed_rs_tag_fire << driver.mesh_completed_rs_tag_fire;
  completion.mesh_completed_bits << driver.mesh_completed_bits;
  for (std::size_t i = 0; i < 2; ++i) {
    completion.pending_completed_set_val[i] << driver.pending_completed_set_val[i];
    completion.pending_completed_set_bits[i] << driver.pending_completed_set_bits[i];
  }

  monitor.cycle << driver.cycle;
  monitor.completed_val << completion.completed_val;
  monitor.completed_bits << completion.completed_bits;
  monitor.pending_completed_val << completion.pending_completed_val;

  Clock clk;
  driver.clk << clk;
  completion.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 16 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_COMPLETION] %s arbitration_and_pending_state\n",
              ok ? "PASS" : "FAIL");
  descore::flushLog();
  return ok ? 0 : 1;
}
