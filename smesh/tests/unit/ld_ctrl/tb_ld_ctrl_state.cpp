// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl_state.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrlState.hpp"

#include <cstdio>

namespace {

class StateDriver : public Component {
  DECLARE_COMPONENT(StateDriver);

 public:
  StateDriver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, head_val);
  Output(bit, do_config);
  Output(u8, config_state_id);
  Output(u64, config_stride);
  Output(u32, config_scale);
  Output(bit, config_shrink);
  Output(u16, config_block_stride);
  Output(u8, config_pixel_repeats);
  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class StateMonitor : public Component {
  DECLARE_COMPONENT(StateMonitor);

 public:
  StateMonitor(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, head_rdy);
  Input(u8, control_state);
  Input(u32, row_counter);
  InputArray(bit, configured, smesh::kLoadStates);
  InputArray(u64, strides, smesh::kLoadStates);
  InputArray(u32, scales, smesh::kLoadStates);
  InputArray(bit, shrinks, smesh::kLoadStates);
  InputArray(u16, block_strides, smesh::kLoadStates);
  InputArray(u8, pixel_repeats, smesh::kLoadStates);
  void update();
  void reset();
  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

StateDriver::StateDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(head_val, do_config, config_state_id, config_stride,
                        config_scale, config_shrink, config_block_stride,
                        config_pixel_repeats);
}

void StateDriver::update() {
  const bool first = cycle_ == 1;
  const bool second = cycle_ == 3;
  head_val = bit(first || second);
  do_config = bit(first || second);
  config_state_id = first ? 2 : 0;
  config_stride = first ? 0x1122334455667788ull : 0x99u;
  config_scale = first ? 0xaabbccddu : 0x1234u;
  config_shrink = bit(first);
  config_block_stride = first ? 0x1357 : 0x2468;
  config_pixel_repeats = first ? 0 : 1;
  ++cycle_;
}

void StateDriver::reset() {
  cycle_ = 0;
  head_val.reset(0);
  do_config.reset(0);
  config_state_id.reset(0);
  config_stride.reset(0);
  config_scale.reset(0);
  config_shrink.reset(0);
  config_block_stride.reset(0);
  config_pixel_repeats.reset(0);
}

StateMonitor::StateMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(head_rdy, control_state, row_counter, configured,
                       strides, scales, shrinks, block_strides)
                .reads(pixel_repeats);
}

void StateMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  const bool first_written = cycle_ >= 2;
  const bool second_written = cycle_ >= 4;
  const bool accepting = cycle_ == 1 || cycle_ == 3;
  passed_ &= (head_rdy == 1) == accepting;
  passed_ &= control_state == static_cast<std::uint8_t>(smesh::LdCtrlFsmState::WaitingForCommand);
  passed_ &= row_counter == 0;
  passed_ &= (configured[2] == 1) == first_written;
  passed_ &= (configured[0] == 1) == second_written;
  passed_ &= configured[1] == 0;
  passed_ &= pixel_repeats[0] == 1 && pixel_repeats[1] == 1 && pixel_repeats[2] == 1;
  if (first_written) {
    passed_ &= strides[2] == 0x1122334455667788ull;
    passed_ &= scales[2] == 0xaabbccddu;
    passed_ &= shrinks[2] == 1;
    passed_ &= block_strides[2] == 0x1357;
  }
  if (second_written) {
    passed_ &= strides[0] == 0x99u;
    passed_ &= scales[0] == 0x1234u;
    passed_ &= shrinks[0] == 0;
    passed_ &= block_strides[0] == 0x2468;
  }

  std::printf("[c%u] accept=%u configured={%u%u%u} pixel={%u,%u,%u}\n",
              cycle_, static_cast<unsigned>(head_rdy == 1),
              static_cast<unsigned>(configured[0] == 1),
              static_cast<unsigned>(configured[1] == 1),
              static_cast<unsigned>(configured[2] == 1),
              static_cast<unsigned>(pixel_repeats[0]),
              static_cast<unsigned>(pixel_repeats[1]),
              static_cast<unsigned>(pixel_repeats[2]));
  done_ = cycle_ == 5;
  ++cycle_;
}

void StateMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LdCtrlState state("LoadState");
  StateDriver driver("Driver");
  StateMonitor monitor("Monitor");
  state.head_val << driver.head_val;
  state.do_config << driver.do_config;
  state.config_state_id << driver.config_state_id;
  state.config_stride << driver.config_stride;
  state.config_scale << driver.config_scale;
  state.config_shrink << driver.config_shrink;
  state.config_block_stride << driver.config_block_stride;
  state.config_pixel_repeats << driver.config_pixel_repeats;
  monitor.head_rdy << state.head_rdy;
  monitor.control_state << state.control_state;
  monitor.row_counter << state.row_counter;
  for (std::size_t i = 0; i < smesh::kLoadStates; ++i) {
    monitor.configured[i] << state.configured[i];
    monitor.strides[i] << state.strides[i];
    monitor.scales[i] << state.scales[i];
    monitor.shrinks[i] << state.shrinks[i];
    monitor.block_strides[i] << state.block_strides[i];
    monitor.pixel_repeats[i] << state.pixel_repeats[i];
  }

  Clock clk;
  state.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL_STATE] %s config\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
