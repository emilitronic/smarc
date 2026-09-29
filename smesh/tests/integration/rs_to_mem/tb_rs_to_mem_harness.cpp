// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_harness.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026

#include "tb_rs_to_mem_harness.hpp"

#include <algorithm>
#include <cstdio>

namespace smesh {
namespace tb {

RsMemCmdDriver::RsMemCmdDriver(const std::vector<SmeshCmd>& program,
                               std::string /*name*/, IMPL_CTOR)
    : program_(program) {
  UPDATE(update).reads(cmd_ready).writes(cmd_valid, cmd_bits);
}

void RsMemCmdDriver::update() {
  cmd_valid = 0;
  cmd_bits = SmeshCmd{};
  if (Sim::state == Sim::SimResetting || done()) {
    return;
  }
  cmd_valid = 1;
  cmd_bits = program_[next_];
  if (cmd_ready == 1) {
    ++next_;
  }
}

void RsMemCmdDriver::reset() {
  next_ = 0;
  cmd_valid.reset(0);
  cmd_bits.reset(SmeshCmd{});
}

CompletionObserver::CompletionObserver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(completed_val, completed_bits,
                       ld_completed_val, ld_completed_bits, ld_completed_rdy,
                       st_completed_val, st_completed_bits, st_completed_rdy);
}

void CompletionObserver::update() {
  if (completed_val == 1) {
    observed_.push_back(*completed_bits);
  }
  if (ld_completed_val == 1 && ld_completed_rdy == 1) {
    observed_.push_back(*ld_completed_bits);
  }
  if (st_completed_val == 1 && st_completed_rdy == 1) {
    observed_.push_back(*st_completed_bits);
  }
}

void CompletionObserver::reset() { observed_.clear(); }

RsMemHarnessInstance::RsMemHarnessInstance(const RsMemTestCase& test,
                                           const std::string& prefix, Clock& clk)
    : test_(test) {
  top_ = std::make_unique<Smesh>(prefix + "Smesh");
  mem_ = std::make_unique<smem::MemCtrl>(prefix + "MemCtrl");
  mem_->set_posted_writes(false);
  dram_ = std::make_unique<smem::Dram>(prefix + "Dram", 0);

  cmd_driver_ = std::make_unique<RsMemCmdDriver>(test_.program, prefix + "CmdDriver");
  completion_observer_ = std::make_unique<CompletionObserver>(prefix + "CompletionObserver");

  top_->cmd_valid << cmd_driver_->cmd_valid;
  top_->cmd_bits  << cmd_driver_->cmd_bits;
  cmd_driver_->cmd_ready << top_->cmd_ready;

  // Connect the DMA reader and writer to the shared simulated DRAM.
  mem_->in_core_req << top_->memReq();
  top_->memResp() << mem_->out_core_resp;
  mem_->in_core_req.setDelay(1);
  dram_->s_req << mem_->s_req;
  mem_->s_resp << dram_->s_resp;

  completion_observer_->completed_val  << top_->exCtrlCompletedVal();
  completion_observer_->completed_bits << top_->exCtrlCompletedBits();
  completion_observer_->ld_completed_val << top_->ldCtrlCompletedVal();
  completion_observer_->ld_completed_bits << top_->ldCtrlCompletedBits();
  completion_observer_->ld_completed_rdy << top_->ldCtrlCompletedRdy();
  completion_observer_->st_completed_val << top_->stCtrlCompletedVal();
  completion_observer_->st_completed_bits << top_->stCtrlCompletedBits();
  completion_observer_->st_completed_rdy << top_->stCtrlCompletedRdy();

  cmd_driver_->clk << clk;
  completion_observer_->clk << clk;
  top_->clk << clk;
  mem_->clk << clk;
  dram_->clk << clk;
}

RsMemHarnessInstance::~RsMemHarnessInstance() = default;

void RsMemHarnessInstance::initializeMemoryImages() {
  for (const auto& row : test_.spad_rows) {
    top_->initializeSpadRow(row.laddr, row.data);
  }
  for (const auto& data : test_.dram_initial) {
    dram_->write(data.addr, data.bytes.data(), data.bytes.size());
  }
}

void RsMemHarnessInstance::sampleBankConcurrency() {
  std::size_t fired = 0;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (top_->spad().read_req_val_bnk[bank] != 0 &&
        top_->spad().read_req_rdy_bnk[bank] != 0) {
      ++fired;
    }
  }
  max_concurrent_spad_banks_ = std::max(max_concurrent_spad_banks_, fired);
}

bool RsMemHarnessInstance::activityComplete() const {
  if (!cmd_driver_->done() || !top_->rs().empty()) return false;
  return !test_.expect_loop_release ||
      (top_->loopMatmul().loop0->configured == 0 &&
       top_->loopMatmul().loop1->configured == 0);
}

bool RsMemHarnessInstance::passed() const {
  auto expected_tags = test_.expected_completion_tags;
  auto observed_tags = completion_observer_->observed();
  std::sort(expected_tags.begin(), expected_tags.end());
  std::sort(observed_tags.begin(), observed_tags.end());
  if (observed_tags != expected_tags) {
    return false;
  }
  if (test_.expect_loop_release &&
      (top_->loopMatmul().loop0->configured == 1 ||
       top_->loopMatmul().loop1->configured == 1)) return false;
  if (test_.min_concurrent_spad_banks > 0 &&
      max_concurrent_spad_banks_ < test_.min_concurrent_spad_banks) {
    return false;
  }
  if (test_.max_concurrent_spad_banks > 0 &&
      max_concurrent_spad_banks_ > test_.max_concurrent_spad_banks) {
    return false;
  }
  for (const auto& expected : test_.expected_results) {
    for (std::size_t r = 0; r < expected.rows.size(); ++r) {
      const auto addr = expected.base + static_cast<std::uint32_t>(r);
      const auto& actual = top_->accum().row(addr);
      for (std::size_t c = 0; c < kDim; ++c) {
        if (actual[c] != expected.rows[r][c]) {
          return false;
        }
      }
    }
  }
  for (const auto& expected : test_.expected_loaded_spad_rows) {
    const auto& actual = top_->spad().row(expected.laddr);
    if (actual != expected.data) return false;
  }
  for (const auto& expected : test_.expected_dram) {
    std::vector<std::uint8_t> actual(expected.bytes.size());
    dram_->read(expected.addr, actual.data(), actual.size());
    if (actual != expected.bytes) return false;
  }
  return true;
}

void RsMemHarnessInstance::report() const {
  std::printf("  rs_empty=%u cmd_driver_done=%u\n",
              top_->rs().empty() ? 1u : 0u, cmd_driver_->done() ? 1u : 0u);
  for (std::size_t i = 0; i < kDefaultConfig.rs_execute_entries; ++i) {
    const auto& entry = top_->rs().executeEntry(i);
    if (!entry.valid) continue;
    std::printf("  ex[%zu] funct=%u tag=%u issued=%u ready=%u deps={%x,%x,%x}\n",
                i, static_cast<unsigned>(entry.cmd.funct),
                static_cast<unsigned>(entry.rs_tag), entry.issued ? 1u : 0u,
                entry.ready() ? 1u : 0u, entry.deps_ld, entry.deps_ex, entry.deps_st);
  }
  std::printf("  expected completions:");
  for (const auto tag : test_.expected_completion_tags) {
    std::printf(" %u", static_cast<unsigned>(tag));
  }
  std::printf("\n  observed completions:");
  for (const auto tag : completion_observer_->observed()) {
    std::printf(" %u", static_cast<unsigned>(tag));
  }
  std::printf("\n");
  std::printf("  max_concurrent_spad_banks observed=%zu expected_min=%zu expected_max=%zu\n",
              max_concurrent_spad_banks_, test_.min_concurrent_spad_banks,
              test_.max_concurrent_spad_banks);
  for (const auto& expected : test_.expected_results) {
    for (std::size_t r = 0; r < expected.rows.size(); ++r) {
      const auto addr = expected.base + static_cast<std::uint32_t>(r);
      const auto& actual = top_->accum().row(addr);
      std::printf("  accum[%u] expected={%d,%d,%d,%d} actual={%d,%d,%d,%d}\n",
                  addr.data(),
                  static_cast<int>(expected.rows[r][0]), static_cast<int>(expected.rows[r][1]),
                  static_cast<int>(expected.rows[r][2]), static_cast<int>(expected.rows[r][3]),
                  static_cast<int>(actual[0]), static_cast<int>(actual[1]),
                  static_cast<int>(actual[2]), static_cast<int>(actual[3]));
    }
  }
  for (const auto& expected : test_.expected_loaded_spad_rows) {
    const auto& actual = top_->spad().row(expected.laddr);
    if (actual != expected.data) {
      std::printf("  spad[%u] loaded row mismatch\n", expected.laddr.data());
    }
  }
  for (const auto& expected : test_.expected_dram) {
    std::vector<std::uint8_t> actual(expected.bytes.size());
    dram_->read(expected.addr, actual.data(), actual.size());
    if (actual == expected.bytes) continue;
    std::printf("  dram[0x%llx] expected=", static_cast<unsigned long long>(expected.addr));
    for (auto byte : expected.bytes) std::printf(" %u", static_cast<unsigned>(byte));
    std::printf(" actual=");
    for (auto byte : actual) std::printf(" %u", static_cast<unsigned>(byte));
    std::printf("\n");
  }
}

} // namespace tb
} // namespace smesh
