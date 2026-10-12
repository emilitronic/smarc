// **********************************************************************
// smesh/tests/unit/loop_conv/tb_loop_conv_load_stream.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LoopConv.hpp"
#include "LoopConvRsCredits.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace {

smesh::SmeshQueuedCmd command(unsigned funct, std::uint64_t rs1, std::uint64_t rs2) {
  smesh::SmeshQueuedCmd value{};
  value.cmd.funct = funct;
  value.cmd.rs1 = rs1;
  value.cmd.rs2 = rs2;
  return value;
}

bool same(const smesh::SmeshCmd& a, const smesh::SmeshCmd& b) {
  return a.funct == b.funct && a.rs1 == b.rs1 && a.rs2 == b.rs2;
}

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, in_val);
  Output(smesh::SmeshQueuedCmd, in_bits);
  Input(bit, in_rdy);
  Output(bit, out_rdy);
  Input(bit, out_val);
  Input(smesh::SmeshQueuedCmd, out_bits);
  Input(smesh::LoopConvSlot, loop0);
  Input(smesh::LoopConvSlot, loop1);
  Input(u16, ld_outstanding);
  Input(u8, head_loop_id);
  Input(bit, busy);
  void updateDrive();
  void updatePosition();
  void updateCheck();
  void updateCycle();
  void reset();
  bool passed() const;
 private:
  std::vector<smesh::SmeshQueuedCmd> program_;
  std::array<std::vector<smesh::SmeshCmd>, 5> expected_;
  Output(u16, position_Q_);
  Register(u16, position_D_);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  std::array<unsigned, 5> seen_{};
  bool passed_ = true;
  bool saw_stall_ = false;
  bool saw_overlap_ = false;
  bool saw_bias_priority_ = false;
  bool saw_weights_priority_ = false;
  bool saw_execute_wait_ = false;
  bool saw_store_wait_ = false;
  bool saw_credit_stall_ = false;
  bool execute_pair_open_ = false;
  unsigned retired_ = 0;
  bool trailing_seen_ = false;
  bool saw_slot_pressure_ = false;
  std::array<smesh::LoopConvSlot, 2> previous_{};
  unsigned previous_head_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  // Three loops reuse slots 0,1,0. Each has four padded input positions,
  // of which the middle two are real. The third setup must wait for a free slot.
  // Each also loads bias into its accumulator half and weights at its SPAD end.
  for (unsigned job = 0; job < 3; ++job) {
    const auto slot = job % 2;
    program_.push_back(command(16, (3ull << 48) | (4ull << 32) | (1ull << 16) | 1,
                               (1ull << 48) | (4ull << 32) | 1));
    program_.push_back(command(17, 1ull << 48, (1ull << 48) | 3));
    program_.push_back(command(18, (1ull << 48) | (1ull << 32) | (4ull << 16) | 1,
                               (1ull << 48) | 2));
    program_.push_back(command(19, (1ull << 48) | 1, (4ull << 48) | (3ull << 32) | (3ull << 16) | 4));
    program_.push_back(command(20, 0x80009000 + job * 0x1000, 0x80003000 + job * 0x1000));
    program_.push_back(command(21, 0x80001000 + job * 0x1000, 0x80005000 + job * 0x1000));
    program_.push_back(command(15, 0, 1));

    const auto acc = slot * smesh::kAccRows / 2;
    const auto spad = slot * smesh::kSpRows / 2;
    expected_[0].push_back(command(0, smesh::packConfigLoadRs1(2, 4, smesh::kMvinScaleIdentityBits, false, 1), 0).cmd);
    expected_[0].push_back(command(14, 0x80001000 + job * 0x1000,
                                   smesh::packLocal(smesh::makeAccAddr(acc), {4, 3})).cmd);
    expected_[1].push_back(command(0, smesh::packConfigLoadRs1(0, 4, smesh::kMvinScaleIdentityBits, false, 1), 4).cmd);
    expected_[1].push_back(command(2, 0, smesh::packLocal(smesh::makeSpAddr(spad), {1, 4})).cmd);
    expected_[1].push_back(command(2, 0x80005000 + job * 0x1000,
                                   smesh::packLocal(smesh::makeSpAddr(spad + 1), {2, 4})).cmd);
    expected_[1].push_back(command(2, 0, smesh::packLocal(smesh::makeSpAddr(spad + 3), {1, 4})).cmd);
    expected_[2].push_back(command(0, smesh::packConfigLoadRs1(1, 4, smesh::kMvinScaleIdentityBits, false, 1), 3).cmd);
    expected_[2].push_back(command(1, 0x80009000 + job * 0x1000,
                                   smesh::packLocal(smesh::makeSpAddr(spad + smesh::kSpRows / 2 - 4), {4, 3})).cmd);
    expected_[3].push_back(command(6,
        smesh::packLocal(smesh::makeSpAddr(spad + smesh::kSpRows / 2 - 4), {4, 3}),
        smesh::packLocal(smesh::makeAccAddr(acc, true), {4, 3})).cmd);
    expected_[3].push_back(command(4, smesh::packLocal(smesh::makeSpAddr(spad), {4, 4}),
        smesh::packLocal(0xffffffffu, {4, 3})).cmd);
    expected_[4].push_back(command(3, 0x80003000 + job * 0x1000,
        smesh::packLocal(smesh::makeAccAddr(acc, false, false), {4, 3})).cmd);
  }
  auto trailing = command(7, 0x12345678, 0xabcdef);
  trailing.rs_tag = 9;
  trailing.rs_tag_valid = 1;
  program_.push_back(trailing);
  position_Q_ <= position_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(position_Q_, cycle_Q_).writes(in_val, in_bits, out_rdy);
  UPDATE(updatePosition).reads(position_Q_, in_val, in_rdy).writes(position_D_);
  UPDATE(updateCheck).reads(out_val, out_bits, out_rdy, loop0, loop1, cycle_Q_, ld_outstanding)
      .reads(head_loop_id, busy, in_val, in_rdy);
  UPDATE(updateCycle).reads(cycle_Q_).writes(cycle_D_);
}

void Driver::updateDrive() {
  const auto p = static_cast<unsigned>(*position_Q_);
  const auto cycle = static_cast<unsigned>(*cycle_Q_);
  in_val = bit(p < program_.size());
  in_bits = p < program_.size() ? program_[p] : smesh::SmeshQueuedCmd{};
  // Stall the initial competing commands, then repeatedly interrupt the drain.
  out_rdy = bit(cycle >= 22 && cycle % 4 != 2);
}

void Driver::updatePosition() {
  if (in_val == 1 && in_rdy == 1) position_D_ = static_cast<unsigned>(*position_Q_) + 1;
}

// Compare each worker's ordered stream; arbitration may interleave their commands.
void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  const auto first = *loop0;
  const std::array<smesh::LoopConvSlot, 2> slots{{first, *loop1}};
  saw_slot_pressure_ |= first.configured == 1 && slots[1].configured == 1 && in_val == 1 && in_rdy == 0;
  for (unsigned id = 0; id < 2; ++id) {
    const auto& old = previous_[id];
    const auto& now = slots[id];
    if (old.configured == 1 && now.configured == 0) {
      // Retirement uses registered completion flags, clears only progress,
      // preserves the saved addresses/geometry, and flips the head once.
      passed_ &= id == previous_head_ && head_loop_id == (previous_head_ ^ 1u) &&
          old.running == 1 && old.ld_bias_completed == 1 && old.ld_input_completed == 1 &&
          old.ld_weights_completed == 1 && old.ex_completed == 1 && old.st_completed == 1 &&
          now.running == 0 && now.ld_bias_started == 0 && now.ld_input_started == 0 &&
          now.ld_weights_started == 0 && now.ex_started == 0 && now.st_started == 0 &&
          now.ld_bias_completed == 0 && now.ld_input_completed == 0 && now.ld_weights_completed == 0 &&
          now.ex_completed == 0 && now.st_completed == 0 &&
          now.a_addr_start == old.a_addr_start && now.b_addr_end == old.b_addr_end &&
          now.weights_dram_addr == old.weights_dram_addr && now.output_dram_addr == old.output_dram_addr &&
          now.bias_dram_addr == old.bias_dram_addr && now.input_dram_addr == old.input_dram_addr &&
          now.inner_bounds.ocols == old.inner_bounds.ocols && now.outer_bounds.out_stride == old.outer_bounds.out_stride &&
          now.launch.no_pool == old.launch.no_pool;
      ++retired_;
    }
  }
  previous_ = slots;
  previous_head_ = unsigned(*head_loop_id);
  if (cycle_Q_ >= 25 && cycle_Q_ < 35 && ld_outstanding == smesh::kDefaultConfig.rs_load_entries) {
    // Downstream can be ready, yet no load worker may exceed its shared limit.
    passed_ &= out_val == 0;
    saw_credit_stall_ |= out_rdy == 1;
  }
  saw_overlap_ |= first.ld_bias_started == 1 && first.ld_input_started == 1 &&
                  first.ld_weights_started == 1 && first.ld_weights_completed == 0 &&
                  first.ld_bias_completed == 0 && first.ld_input_completed == 0;
  saw_execute_wait_ |= first.ex_started == 1 &&
      !(first.ld_bias_completed == 1 && first.ld_input_completed == 1 && first.ld_weights_completed == 1);
  saw_store_wait_ |= first.st_started == 1 && first.ex_completed == 0;
  if (out_val == 0) return;
  if (out_rdy == 0) {
    saw_stall_ = true;
    return;
  }
  const auto value = *out_bits;
  const auto funct = static_cast<unsigned>(value.cmd.funct);
  if (funct == 7) {
    const auto& expected = program_.back();
    passed_ &= !trailing_seen_ && retired_ == 3 && same(value.cmd, expected.cmd) &&
        value.rs_tag == expected.rs_tag && value.rs_tag_valid == 1 &&
        value.from_conv_loop == 0 && value.from_mmul_loop == 0 &&
        first.configured == 0 && slots[1].configured == 0;
    for (unsigned worker = 0; worker < 5; ++worker) passed_ &= seen_[worker] == expected_[worker].size();
    trailing_seen_ = true;
    return;
  }
  const bool bias = funct == 14 || (funct == 0 && ((std::uint64_t(value.cmd.rs1) >> 3) & 3) == 2);
  const bool weights = funct == 1 || (funct == 0 && ((std::uint64_t(value.cmd.rs1) >> 3) & 3) == 1);
  const bool execute = funct == 6 || funct == 4 || funct == 5;
  const bool store = funct == 3;
  const unsigned worker = store ? 4 : execute ? 3 : bias ? 0 : weights ? 2 : 1;
  const auto n = seen_[worker];
  const bool match = n < expected_[worker].size() && same(value.cmd, expected_[worker][n]);
  passed_ &= match && value.from_conv_loop == 1 && value.from_mmul_loop == 0 && value.rs_tag_valid == 0;
  if (!match) std::printf("Load stream mismatch worker=%u command=%u funct=%u rs1=0x%llx rs2=0x%llx\n",
                         worker, n, funct, static_cast<unsigned long long>(value.cmd.rs1),
                         static_cast<unsigned long long>(value.cmd.rs2));
  if (seen_[0] == 0 && seen_[1] == 0 && seen_[2] == 0 && seen_[3] == 0) saw_bias_priority_ = bias;
  // Both lower-priority workers were stalled with commands pending. Weights
  // must win their first transfer before input can transfer anything.
  if (weights && seen_[2] == 0) saw_weights_priority_ = seen_[1] == 0;
  if (worker == 1 && seen_[1] == 0) passed_ &= seen_[2] >= 2;
  const auto job = store ? n : worker == 1 ? n / 4 : n / 2;
  const auto slot = slots[job % 2];
  if (!execute && !store && job > 0) {
    // Later load jobs must not generate ahead while Execute awaits the preceding
    // job's loads. This scenario has no prequeued data for that later job.
    passed_ &= seen_[0] >= job * 2 && seen_[1] >= job * 4 && seen_[2] >= job * 2;
  }
  passed_ &= store ? slot.st_completed == 0 : execute ? slot.ex_completed == 0 : bias ? slot.ld_bias_completed == 0 : weights
      ? slot.ld_weights_completed == 0 : slot.ld_input_completed == 0;
  // Only higher-priority Store may interrupt a pipelined PRELOAD/COMPUTE pair.
  if (execute_pair_open_) passed_ &= execute || store;
  if (store) passed_ &= seen_[3] >= (job + 1) * 2 && slot.ex_completed == 1;
  if (execute) {
    const auto job = n / 2;
    passed_ &= seen_[0] >= (job + 1) * 2 && seen_[1] >= (job + 1) * 4 && seen_[2] >= (job + 1) * 2;
    execute_pair_open_ = funct == 6;
  }
  ++seen_[worker];
}

void Driver::updateCycle() { cycle_D_ = static_cast<unsigned>(*cycle_Q_) + 1; }

void Driver::reset() {
  position_D_.reset(0);
  cycle_D_.reset(0);
  in_val.reset(0);
  in_bits.reset(smesh::SmeshQueuedCmd{});
  out_rdy.reset(0);
  seen_ = {};
  passed_ = true;
  saw_stall_ = saw_overlap_ = saw_bias_priority_ = false;
  saw_weights_priority_ = false;
  saw_execute_wait_ = execute_pair_open_ = false;
  saw_store_wait_ = false;
  saw_credit_stall_ = false;
  retired_ = 0;
  trailing_seen_ = saw_slot_pressure_ = false;
  previous_ = {};
  previous_head_ = 0;
}

bool Driver::passed() const {
  const auto a = *loop0;
  const auto b = *loop1;
  return passed_ && retired_ == 3 && trailing_seen_ && saw_slot_pressure_ &&
         saw_credit_stall_ && saw_stall_ && saw_overlap_ && saw_bias_priority_ && saw_weights_priority_ && saw_execute_wait_ && saw_store_wait_ &&
         seen_[0] == expected_[0].size() && seen_[1] == expected_[1].size() &&
         seen_[2] == expected_[2].size() &&
         seen_[3] == expected_[3].size() && !execute_pair_open_ &&
         seen_[4] == expected_[4].size() &&
         *position_Q_ == program_.size() &&
         a.configured == 0 && b.configured == 0 && busy == 0 && head_loop_id == 1 &&
         a.output_dram_addr == 0x80005000 && b.output_dram_addr == 0x80004000;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::LoopConv loop("LoopConv");
  LoopConvRsCredits credits("Credits", 35);
  loop.in_val << driver.in_val;
  loop.in_bits << driver.in_bits;
  driver.in_rdy << loop.in_rdy;
  loop.out_rdy << driver.out_rdy;
  driver.out_val << loop.out_val;
  driver.out_bits << loop.out_bits;
  driver.loop0 << loop.loop0;
  driver.loop1 << loop.loop1;
  driver.ld_outstanding << loop.ld_outstanding;
  driver.head_loop_id << loop.head_loop_id;
  driver.busy << loop.busy;
  Clock clk;
  credits.connect(loop, clk);
  driver.clk << clk;
  loop.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool passed = true;
  for (unsigned run = 0; run < 2; ++run) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 180; ++cycle) Sim::run();
    passed &= driver.passed();
  }
  descore::flushLog();
  std::printf("[LOOP_CONV_LOAD_STREAM] %s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
