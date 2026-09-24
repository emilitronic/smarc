// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl_cmd_tracker.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrlCmdTracker.hpp"

#include <array>
#include <cstdio>

namespace {

struct Stimulus {
  bool alloc = false;
  std::uint32_t alloc_bytes = 0;
  smesh::SmeshRsTag tag = 0;
  bool returned = false;
  std::uint16_t returned_id = 0;
  std::uint32_t returned_bytes = 0;
  bool complete_ready = false;
};

Stimulus stimulus(unsigned cycle) {
  if (cycle < smesh::kLoadCmdTrackerEntries) {
    return {true, cycle == 0 ? 8u : cycle == 1 ? 4u : 1u,
            static_cast<smesh::SmeshRsTag>((cycle + 1) * 10)};
  }
  switch (cycle) {
    case 5:  return {true, 2, 60, true, 1, 2};
    case 6:  return {false, 0, 0, true, 1, 2};
    case 7:  return {false, 0, 0, true, 0, 3};
    case 8:  return {false, 0, 0, false, 0, 0, true};
    case 9:  return {true, 2, 60, true, 0, 5};
    case 10: return {false, 0, 0, true, 1, 2};
    case 11: return {false, 0, 0, false, 0, 0, true};
    case 12: return {false, 0, 0, false, 0, 0, true};
    case 13: return {false, 0, 0, true, 2, 1};
    case 14: return {false, 0, 0, true, 3, 1};
    case 15: return {false, 0, 0, false, 0, 0, true};
    case 16: return {false, 0, 0, false, 0, 0, true};
    case 17: return {false, 0, 0, true, 4, 1};
    case 18: return {false, 0, 0, false, 0, 0, true};
    default: return {};
  }
}

class TrackerDriver : public Component {
  DECLARE_COMPONENT(TrackerDriver);

 public:
  TrackerDriver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, alloc_val);
  Output(u32, alloc_bytes_to_read);
  Output(smesh::SmeshRsTag, alloc_rs_tag);
  Output(bit, returned_val);
  Output(u16, returned_cmd_id);
  Output(u32, returned_bytes_read);
  Output(bit, completed_rdy);
  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class TrackerMonitor : public Component {
  DECLARE_COMPONENT(TrackerMonitor);

 public:
  TrackerMonitor(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, alloc_rdy);
  Input(u16, alloc_cmd_id);
  Input(bit, completed_val);
  Input(smesh::SmeshRsTag, completed_bits);
  Input(u16, completed_cmd_id);
  Input(bit, busy);
  void update();
  void reset();
  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  struct ExpectedEntry {
    bool valid = false;
    smesh::SmeshRsTag tag = 0;
    std::uint32_t bytes_left = 0;
  };
  std::array<ExpectedEntry, smesh::kLoadCmdTrackerEntries> expected_{};
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

TrackerDriver::TrackerDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(alloc_val, alloc_bytes_to_read, alloc_rs_tag,
                        returned_val, returned_cmd_id, returned_bytes_read,
                        completed_rdy);
}

void TrackerDriver::update() {
  const auto s = stimulus(cycle_++);
  alloc_val = bit(s.alloc);
  alloc_bytes_to_read = s.alloc_bytes;
  alloc_rs_tag = s.tag;
  returned_val = bit(s.returned);
  returned_cmd_id = s.returned_id;
  returned_bytes_read = s.returned_bytes;
  completed_rdy = bit(s.complete_ready);
}

void TrackerDriver::reset() {
  cycle_ = 0;
  alloc_val.reset(0);
  alloc_bytes_to_read.reset(0);
  alloc_rs_tag.reset(0);
  returned_val.reset(0);
  returned_cmd_id.reset(0);
  returned_bytes_read.reset(0);
  completed_rdy.reset(0);
}

TrackerMonitor::TrackerMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(alloc_rdy, alloc_cmd_id, completed_val,
                       completed_bits, completed_cmd_id, busy);
}

void TrackerMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  bool free = false;
  bool complete = false;
  bool active = false;
  std::size_t free_id = 0;
  std::size_t complete_id = 0;
  for (std::size_t i = 0; i < expected_.size(); ++i) {
    const auto& entry = expected_[i];
    active |= entry.valid;
    if (!free && !entry.valid) {
      free = true;
      free_id = i;
    }
    if (!complete && entry.valid && entry.bytes_left == 0) {
      complete = true;
      complete_id = i;
    }
  }

  passed_ &= (alloc_rdy == 1) == free;
  passed_ &= static_cast<std::size_t>(alloc_cmd_id) == free_id;
  passed_ &= (completed_val == 1) == complete;
  passed_ &= static_cast<std::size_t>(completed_cmd_id) == complete_id;
  passed_ &= static_cast<smesh::SmeshRsTag>(completed_bits) ==
             (complete ? expected_[complete_id].tag : 0);
  passed_ &= (busy == 1) == active;
  std::printf("[c%02u] alloc{r=%u id=%u} complete{v=%u id=%u tag=%u} busy=%u\n",
              cycle_, static_cast<unsigned>(alloc_rdy == 1),
              static_cast<unsigned>(alloc_cmd_id),
              static_cast<unsigned>(completed_val == 1),
              static_cast<unsigned>(completed_cmd_id),
              static_cast<unsigned>(completed_bits),
              static_cast<unsigned>(busy == 1));

  const auto s = stimulus(cycle_);
  if (complete && s.complete_ready) {
    expected_[complete_id] = ExpectedEntry{};
  }
  if (s.returned) {
    passed_ &= s.returned_id < expected_.size() &&
               expected_[s.returned_id].valid &&
               expected_[s.returned_id].bytes_left >= s.returned_bytes;
    expected_[s.returned_id].bytes_left -= s.returned_bytes;
  }
  if (s.alloc && free) {
    expected_[free_id] = ExpectedEntry{true, s.tag, s.alloc_bytes};
  }
  done_ = cycle_ == 19;
  ++cycle_;
}

void TrackerMonitor::reset() {
  expected_ = {};
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LdCtrlCmdTracker tracker("LoadTracker");
  TrackerDriver driver("Driver");
  TrackerMonitor monitor("Monitor");
  tracker.alloc_val << driver.alloc_val;
  tracker.alloc_bytes_to_read << driver.alloc_bytes_to_read;
  tracker.alloc_rs_tag << driver.alloc_rs_tag;
  tracker.returned_val << driver.returned_val;
  tracker.returned_cmd_id << driver.returned_cmd_id;
  tracker.returned_bytes_read << driver.returned_bytes_read;
  tracker.completed_rdy << driver.completed_rdy;
  monitor.alloc_rdy << tracker.alloc_rdy;
  monitor.alloc_cmd_id << tracker.alloc_cmd_id;
  monitor.completed_val << tracker.completed_val;
  monitor.completed_bits << tracker.completed_bits;
  monitor.completed_cmd_id << tracker.completed_cmd_id;
  monitor.busy << tracker.busy;

  Clock clk;
  tracker.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 22 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL_CMD_TRACKER] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
