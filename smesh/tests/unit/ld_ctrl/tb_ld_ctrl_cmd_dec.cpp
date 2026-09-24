// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl_cmd_dec.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrlCmdDec.hpp"
#include "SmeshCommand.hpp"

#include <cstdio>

namespace {

constexpr std::uint64_t kVaddr = 0x123456789abcdef0ull;
constexpr std::uint64_t kConfigStride = 0x1122334455667788ull;
constexpr std::uint32_t kScale = 0xa1b2c3d4u;
constexpr std::uint16_t kBlockStride = 0x5678u;
constexpr std::uint8_t kPixelRepeats = 0x9au;
constexpr std::uint32_t kLocalAddr = 0x30000009u;

class DecDriver : public Component {
  DECLARE_COMPONENT(DecDriver);

 public:
  DecDriver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, head_val);
  Output(smesh::SmeshIssue, head_bits);
  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class DecMonitor : public Component {
  DECLARE_COMPONENT(DecMonitor);

 public:
  DecMonitor(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, do_config);
  Input(bit, do_load);
  Input(u8, load_state_id);
  Input(u8, config_state_id);
  Input(u8, state_id);
  Input(u64, vaddr);
  Input(smesh::SmeshLocalAddr, localaddr);
  Input(u32, rows);
  Input(u32, cols);
  Input(u64, config_stride);
  Input(u32, config_scale);
  Input(bit, config_shrink);
  Input(u16, config_block_stride);
  Input(u8, config_pixel_repeats);
  void update();
  void reset();
  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

DecDriver::DecDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(head_val, head_bits);
}

void DecDriver::update() {
  smesh::SmeshIssue issue{};
  head_val = bit(cycle_ != 0);
  if (cycle_ == 1) {
    issue.cmd.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::Config);
    issue.cmd.rs1 = static_cast<std::uint64_t>(smesh::ConfigKind::Load) |
                    (1ull << 2) | (2ull << 3) |
                    (static_cast<std::uint64_t>(kPixelRepeats) << 8) |
                    (static_cast<std::uint64_t>(kBlockStride) << 16) |
                    (static_cast<std::uint64_t>(kScale) << 32);
    issue.cmd.rs2 = kConfigStride;
  } else {
    issue.cmd.funct = static_cast<std::uint32_t>(
        cycle_ == 3 ? smesh::SmeshFunct::Mvin2 :
        cycle_ == 4 ? smesh::SmeshFunct::Mvin3 : smesh::SmeshFunct::Mvin);
    issue.cmd.rs1 = kVaddr;
    issue.cmd.rs2 = smesh::packLocal(kLocalAddr, {3, 5});
  }
  head_bits = issue;
  ++cycle_;
}

void DecDriver::reset() {
  cycle_ = 0;
  head_val.reset(0);
  head_bits.reset(smesh::SmeshIssue{});
}

DecMonitor::DecMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(do_config, do_load, load_state_id, config_state_id,
                       state_id, vaddr, localaddr, rows)
                .reads(cols)
                .reads(config_stride, config_scale, config_shrink,
                       config_block_stride, config_pixel_repeats);
}

void DecMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  const bool config = cycle_ == 1;
  const bool load = cycle_ >= 2;
  const unsigned expected_load_slot = cycle_ == 3 ? 1 : cycle_ == 4 ? 2 : 0;
  const unsigned expected_slot = config ? 2 : expected_load_slot;

  passed_ &= (do_config == 1) == config;
  passed_ &= (do_load == 1) == load;
  passed_ &= static_cast<unsigned>(load_state_id) == expected_load_slot;
  passed_ &= static_cast<unsigned>(config_state_id) == (config ? 2u : 0u);
  passed_ &= static_cast<unsigned>(state_id) == expected_slot;
  passed_ &= static_cast<std::uint64_t>(vaddr) == (load ? kVaddr : 0);
  passed_ &= (*localaddr).raw == (load ? kLocalAddr : 0);
  passed_ &= static_cast<std::uint32_t>(rows) == (load ? 3u : 0u);
  passed_ &= static_cast<std::uint32_t>(cols) == (load ? 5u : 0u);
  passed_ &= static_cast<std::uint64_t>(config_stride) == (config ? kConfigStride : 0);
  passed_ &= static_cast<std::uint32_t>(config_scale) == (config ? kScale : 0);
  passed_ &= (config_shrink == 1) == config;
  passed_ &= static_cast<std::uint16_t>(config_block_stride) == (config ? kBlockStride : 0);
  passed_ &= static_cast<std::uint8_t>(config_pixel_repeats) == (config ? kPixelRepeats : 0);

  std::printf("[c%u] cfg=%u load=%u slot=%u addr=%08x shape=%u,%u\n",
              cycle_, static_cast<unsigned>(do_config == 1),
              static_cast<unsigned>(do_load == 1), static_cast<unsigned>(state_id),
              (*localaddr).raw, static_cast<unsigned>(rows), static_cast<unsigned>(cols));
  done_ = cycle_ == 4;
  ++cycle_;
}

void DecMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LdCtrlCmdDec decoder("LoadDecoder");
  DecDriver driver("Driver");
  DecMonitor monitor("Monitor");
  decoder.head_val << driver.head_val;
  decoder.head_bits << driver.head_bits;
  monitor.do_config << decoder.do_config;
  monitor.do_load << decoder.do_load;
  monitor.load_state_id << decoder.load_state_id;
  monitor.config_state_id << decoder.config_state_id;
  monitor.state_id << decoder.state_id;
  monitor.vaddr << decoder.vaddr;
  monitor.localaddr << decoder.localaddr;
  monitor.rows << decoder.rows;
  monitor.cols << decoder.cols;
  monitor.config_stride << decoder.config_stride;
  monitor.config_scale << decoder.config_scale;
  monitor.config_shrink << decoder.config_shrink;
  monitor.config_block_stride << decoder.config_block_stride;
  monitor.config_pixel_repeats << decoder.config_pixel_repeats;

  Clock clk;
  decoder.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 7 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL_CMD_DEC] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
