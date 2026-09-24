// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl_state_fsm.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrlState.hpp"

#include <array>
#include <cstdio>

namespace {

struct Cycle {
  bool head;
  bool alloc_ready;
  std::uint16_t alloc_id;
  bool req_ready;
  std::uint32_t actual_rows;
  std::uint32_t rows;
  smesh::LdCtrlFsmState state;
  std::uint32_t row;
  bool alloc_valid;
  bool req_valid;
  std::uint16_t req_id;
  bool pop;
};

const std::array<Cycle, 16> kCycles{{
    {0, 0, 0, 0, 3, 3, smesh::LdCtrlFsmState::WaitingForCommand,     0, 0, 0, 0, 0},
    {1, 0, 3, 0, 3, 3, smesh::LdCtrlFsmState::WaitingForCommand,     0, 1, 0, 0, 0},
    {1, 1, 3, 0, 3, 3, smesh::LdCtrlFsmState::WaitingForCommand,     0, 1, 1, 3, 0},
    {1, 0, 0, 0, 3, 3, smesh::LdCtrlFsmState::WaitingForDmaReqReady, 0, 0, 1, 3, 0},
    {1, 0, 0, 1, 3, 3, smesh::LdCtrlFsmState::WaitingForDmaReqReady, 0, 0, 1, 3, 0},
    {1, 0, 0, 0, 3, 3, smesh::LdCtrlFsmState::SendingRows,           1, 0, 1, 3, 0},
    {1, 0, 0, 1, 3, 3, smesh::LdCtrlFsmState::SendingRows,           1, 0, 1, 3, 0},
    {1, 0, 0, 1, 3, 3, smesh::LdCtrlFsmState::SendingRows,           2, 0, 1, 3, 1},
    {0, 0, 0, 0, 3, 3, smesh::LdCtrlFsmState::WaitingForCommand,     0, 0, 0, 3, 0},
    {1, 1, 4, 1, 1, 4, smesh::LdCtrlFsmState::WaitingForCommand,     0, 1, 1, 4, 0},
    {1, 0, 0, 0, 1, 4, smesh::LdCtrlFsmState::SendingRows,           0, 0, 0, 4, 1},
    {0, 0, 0, 0, 1, 4, smesh::LdCtrlFsmState::WaitingForCommand,     0, 0, 0, 4, 0},
    {1, 1, 5, 1, 2, 2, smesh::LdCtrlFsmState::WaitingForCommand,     0, 1, 1, 5, 0},
    {1, 0, 0, 0, 2, 2, smesh::LdCtrlFsmState::SendingRows,           1, 0, 1, 5, 0},
    {1, 0, 0, 1, 2, 2, smesh::LdCtrlFsmState::SendingRows,           1, 0, 1, 5, 1},
    {0, 0, 0, 0, 2, 2, smesh::LdCtrlFsmState::WaitingForCommand,     0, 0, 0, 5, 0},
}};

class FsmDriver : public Component {
  DECLARE_COMPONENT(FsmDriver);

 public:
  FsmDriver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, head_val);
  Output(bit, do_config);
  Output(bit, do_load);
  Output(bit, tracker_alloc_rdy);
  Output(u16, tracker_alloc_cmd_id);
  Output(bit, dma_req_rdy);
  Output(u32, actual_rows_read);
  Output(u32, rows);
  Output(u16, block_stride);
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

class FsmMonitor : public Component {
  DECLARE_COMPONENT(FsmMonitor);

 public:
  FsmMonitor(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, head_rdy);
  Input(bit, tracker_alloc_val);
  Input(bit, dma_req_val);
  Input(u16, dma_req_cmd_id);
  Input(u8, control_state);
  Input(u32, row_counter);
  void update();
  void reset();
  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

FsmDriver::FsmDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(head_val, do_config, do_load, tracker_alloc_rdy,
              tracker_alloc_cmd_id, dma_req_rdy, actual_rows_read, rows)
      .writes(block_stride, config_state_id, config_stride, config_scale,
              config_shrink, config_block_stride, config_pixel_repeats);
}

void FsmDriver::update() {
  const auto& c = kCycles[cycle_ < kCycles.size() ? cycle_ : kCycles.size() - 1];
  head_val = bit(c.head);
  do_config = 0;
  do_load = bit(c.head);
  tracker_alloc_rdy = bit(c.alloc_ready);
  tracker_alloc_cmd_id = c.alloc_id;
  dma_req_rdy = bit(c.req_ready);
  actual_rows_read = c.actual_rows;
  rows = c.rows;
  block_stride = 4;
  config_state_id = 0;
  config_stride = 0;
  config_scale = 0;
  config_shrink = 0;
  config_block_stride = 0;
  config_pixel_repeats = 0;
  ++cycle_;
}

void FsmDriver::reset() {
  cycle_ = 0;
  head_val.reset(0);
  do_config.reset(0);
  do_load.reset(0);
  tracker_alloc_rdy.reset(0);
  tracker_alloc_cmd_id.reset(0);
  dma_req_rdy.reset(0);
  actual_rows_read.reset(0);
  rows.reset(0);
  block_stride.reset(0);
  config_state_id.reset(0);
  config_stride.reset(0);
  config_scale.reset(0);
  config_shrink.reset(0);
  config_block_stride.reset(0);
  config_pixel_repeats.reset(0);
}

FsmMonitor::FsmMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(head_rdy, tracker_alloc_val, dma_req_val,
                       dma_req_cmd_id, control_state, row_counter);
}

void FsmMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  const auto& c = kCycles[cycle_];
  passed_ &= (head_rdy == 1) == c.pop;
  passed_ &= (tracker_alloc_val == 1) == c.alloc_valid;
  passed_ &= (dma_req_val == 1) == c.req_valid;
  passed_ &= static_cast<std::uint16_t>(dma_req_cmd_id) == c.req_id;
  passed_ &= static_cast<std::uint8_t>(control_state) ==
             static_cast<std::uint8_t>(c.state);
  passed_ &= static_cast<std::uint32_t>(row_counter) == c.row;

  std::printf("[c%02u] state=%u row=%u alloc=%u req={v=%u id=%u} pop=%u\n",
              cycle_, static_cast<unsigned>(control_state),
              static_cast<unsigned>(row_counter),
              static_cast<unsigned>(tracker_alloc_val == 1),
              static_cast<unsigned>(dma_req_val == 1),
              static_cast<unsigned>(dma_req_cmd_id),
              static_cast<unsigned>(head_rdy == 1));
  done_ = cycle_ + 1 == kCycles.size();
  ++cycle_;
}

void FsmMonitor::reset() {
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
  FsmDriver driver("Driver");
  FsmMonitor monitor("Monitor");
  state.head_val << driver.head_val;
  state.do_config << driver.do_config;
  state.do_load << driver.do_load;
  state.tracker_alloc_rdy << driver.tracker_alloc_rdy;
  state.tracker_alloc_cmd_id << driver.tracker_alloc_cmd_id;
  state.dma_req_rdy << driver.dma_req_rdy;
  state.actual_rows_read << driver.actual_rows_read;
  state.rows << driver.rows;
  state.block_stride << driver.block_stride;
  state.config_state_id << driver.config_state_id;
  state.config_stride << driver.config_stride;
  state.config_scale << driver.config_scale;
  state.config_shrink << driver.config_shrink;
  state.config_block_stride << driver.config_block_stride;
  state.config_pixel_repeats << driver.config_pixel_repeats;
  monitor.head_rdy << state.head_rdy;
  monitor.tracker_alloc_val << state.tracker_alloc_val;
  monitor.dma_req_val << state.dma_req_val;
  monitor.dma_req_cmd_id << state.dma_req_cmd_id;
  monitor.control_state << state.control_state;
  monitor.row_counter << state.row_counter;

  Clock clk;
  state.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 19 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL_STATE_FSM] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
