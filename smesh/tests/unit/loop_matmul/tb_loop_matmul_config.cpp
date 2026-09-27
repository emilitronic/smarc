// **********************************************************************
// smesh/tests/unit/loop_matmul/tb_loop_matmul_config.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026
// Focused test checks reset boundaries, pass-through backpressure, 
// both slot contents, and stalling when the slots and queu are full.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopMatmul.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

smesh::SmeshQueuedCmd command(smesh::SmeshFunct funct, std::uint64_t rs1,
                              std::uint64_t rs2) {
  smesh::SmeshQueuedCmd queued{};
  queued.cmd.funct = static_cast<std::uint32_t>(funct);
  queued.cmd.rs1 = rs1;
  queued.cmd.rs2 = rs2;
  return queued;
}

class LoopDriver : public Component {
  DECLARE_COMPONENT(LoopDriver);

 public:
  LoopDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, in_val);
  Output(smesh::SmeshQueuedCmd, in_bits);
  Input(bit, in_rdy);
  Input(bit, out_val);
  Input(smesh::SmeshQueuedCmd, out_bits);
  Output(bit, out_rdy);

  void updateCommand();
  void updateReady();
  void observeOutput();
  void reset();

  std::size_t accepted() const { return program_pos_; }
  unsigned forwarded() const { return forwarded_; }
  bool sawStall() const { return saw_stall_; }
  bool passed() const { return passed_; }

 private:
  std::array<smesh::SmeshQueuedCmd, 16> program_{};
  std::size_t program_pos_ = 0;
  unsigned cycle_ = 0;
  unsigned forwarded_ = 0;
  bool saw_stall_ = false;
  bool passed_ = true;
};

LoopDriver::LoopDriver(std::string /*name*/, IMPL_CTOR) {
  auto ordinary = command(smesh::SmeshFunct::Mvin, 0xabc0, 0x1234);
  ordinary.rs_tag = 7;
  ordinary.rs_tag_valid = 1;
  ordinary.from_conv_loop = 1;
  program_ = {{
      ordinary,
      command(smesh::SmeshFunct::LoopWsBounds, (3ull << 32) | (2ull << 16) | 1ull,
              (1ull << 32) | (1ull << 16) | 2ull),
      command(smesh::SmeshFunct::LoopWsAddrsAb, 0x1000, 0x2000),
      command(smesh::SmeshFunct::LoopWsAddrsDc, 0x3000, 0x4000),
      command(smesh::SmeshFunct::LoopWsStridesAb, 64, 96),
      command(smesh::SmeshFunct::LoopWsStridesDc, 128, 160),
      command(smesh::SmeshFunct::LoopWs, 1ull | (2ull << 16), (1ull << 3) | (1ull << 8)),
      command(smesh::SmeshFunct::LoopWsBounds, 0, (1ull << 32) | (2ull << 16) | 1ull),
      command(smesh::SmeshFunct::LoopWsAddrsAb, 0x5000, 0x6000),
      command(smesh::SmeshFunct::LoopWsAddrsDc, 0x7000, 0x8000),
      command(smesh::SmeshFunct::LoopWsStridesAb, 192, 224),
      command(smesh::SmeshFunct::LoopWsStridesDc, 256, 288),
      command(smesh::SmeshFunct::LoopWs, 1ull << 17, (1ull << 1) | (1ull << 9)),
      command(smesh::SmeshFunct::LoopWsBounds, 0, (1ull << 32) | (1ull << 16) | 1ull),
      command(smesh::SmeshFunct::LoopWsAddrsAb, 0x9000, 0xa000),
      command(smesh::SmeshFunct::LoopWsAddrsDc, 0xb000, 0xc000),
  }};

  UPDATE(updateReady).writes(out_rdy);
  UPDATE(updateCommand).reads(in_rdy).writes(in_val, in_bits);
  UPDATE(observeOutput).reads(out_val, out_bits, out_rdy);
}

void LoopDriver::updateReady() {
  out_rdy = bit(Sim::state != Sim::SimResetting && cycle_ >= 4);
}

void LoopDriver::updateCommand() {
  if (Sim::state == Sim::SimResetting) {
    in_val = 0;
    in_bits = smesh::SmeshQueuedCmd{};
    return;
  }
  const bool offering = program_pos_ < program_.size();
  in_val = bit(offering);
  in_bits = offering ? program_[program_pos_] : smesh::SmeshQueuedCmd{};
  if (offering && in_rdy == 1) ++program_pos_;
}

void LoopDriver::observeOutput() {
  if (Sim::state == Sim::SimResetting) return;
  if (out_val == 1 && out_rdy == 0) saw_stall_ = true;
  if (out_val == 1 && out_rdy == 1) {
    const auto value = *out_bits;
    passed_ &= static_cast<std::uint32_t>(value.cmd.funct) ==
                   static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin) &&
               static_cast<std::uint64_t>(value.cmd.rs1) == 0xabc0 &&
               value.rs_tag == 7 && value.rs_tag_valid == 1 &&
               value.from_conv_loop == 1;
    ++forwarded_;
  }
  ++cycle_;
}

void LoopDriver::reset() {
  program_pos_ = 0;
  cycle_ = 0;
  forwarded_ = 0;
  saw_stall_ = false;
  passed_ = true;
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  LoopDriver driver("Driver");
  smesh::LoopMatmul loop("LoopMatmul");
  loop.in_val << driver.in_val;
  loop.in_bits << driver.in_bits;
  driver.in_rdy << loop.in_rdy;
  loop.out_rdy << driver.out_rdy;
  driver.out_val << loop.out_val;
  driver.out_bits << loop.out_bits;

  Clock clk;
  driver.clk << clk;
  loop.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  const auto reset0 = *loop.loop0;
  const auto reset1 = *loop.loop1;
  bool passed = reset0.configured == 0 && reset1.configured == 0 &&
                reset0.a_addr_start == 0 && reset0.b_addr_end == smesh::kSpRows / 2 &&
                reset1.a_addr_start == smesh::kSpRows / 2 && reset1.b_addr_end == smesh::kSpRows;

  for (int cycle = 0; cycle < 45; ++cycle) Sim::run();

  const auto first = *loop.loop0;
  const auto second = *loop.loop1;
  passed &= driver.passed() && driver.sawStall() && driver.forwarded() == 1;
  passed &= driver.accepted() == 15 && *loop.in_rdy == 0 && *loop.busy == 1;
  passed &= *loop.head_loop_id == 0;
  passed &= first.configured == 1 && first.max_i == 2 && first.max_j == 1 && first.max_k == 1 &&
            first.pad_i == 1 && first.pad_j == 2 && first.pad_k == 3 &&
            first.a_dram_addr == 0x1000 && first.b_dram_addr == 0x2000 &&
            first.d_dram_addr == 0x3000 && first.c_dram_addr == 0x4000 &&
            first.a_dram_stride == 64 && first.b_dram_stride == 96 &&
            first.d_dram_stride == 128 && first.c_dram_stride == 160 &&
            first.a_addr_start == 0 && first.b_addr_end == smesh::kSpRows / 2 &&
            first.ex_accumulate == 1 && first.b_ex_spad_id == 2 &&
            first.lda_started == 1 && first.lda_completed == 1 && first.inc_acc_addr == 1;
  passed &= second.configured == 1 && second.max_i == 1 && second.max_j == 2 && second.max_k == 1 &&
            second.a_dram_addr == 0x5000 && second.b_dram_addr == 0x6000 &&
            second.d_dram_addr == 0x7000 && second.c_dram_addr == 0x8000 &&
            second.a_dram_stride == 192 && second.b_dram_stride == 224 &&
            second.d_dram_stride == 256 && second.c_dram_stride == 288 &&
            second.a_addr_start == smesh::kSpRows / 2 && second.b_addr_end == smesh::kSpRows &&
            second.b_ex_spad_id == 2 && second.b_transpose == 1 && second.spad_only == 1;

  descore::flushLog();
  std::printf("[LOOP_MATMUL_CONFIG] %s accepted=%zu forwarded=%u slots=%u%u\n",
              passed ? "PASS" : "FAIL", driver.accepted(), driver.forwarded(),
              static_cast<unsigned>(first.configured == 1),
              static_cast<unsigned>(second.configured == 1));
  return passed ? 0 : 1;
}
