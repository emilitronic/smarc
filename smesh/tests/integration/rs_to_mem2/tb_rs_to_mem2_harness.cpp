// **********************************************************************
// smesh/tests/integration/rs_to_mem2/tb_rs_to_mem2_harness.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026

#include "tb_rs_to_mem2_harness.hpp"

#include <algorithm>
#include <cstdio>

namespace smesh {
namespace tb {

RsMem2HarnessInstance::RsMem2HarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk)
    : test_(test) {
  top_ = std::make_unique<Smesh>(prefix + "Smesh");
  mem_ = std::make_unique<smem::MemCtrl>(prefix + "MemCtrl");
  dram_ = std::make_unique<smem::Dram>(prefix + "Dram", 0);

  cmd_driver_ = std::make_unique<RsMemCmdDriver>(test_.program, prefix + "CmdDriver");
  spad_preload_ = std::make_unique<SpadPreloadDriver>(test_.spad_rows, prefix + "SpadPreload");
  completion_observer_ = std::make_unique<CompletionObserver>(prefix + "CompletionObserver");

  top_->cmd_valid << cmd_driver_->cmd_valid;
  top_->cmd_bits  << cmd_driver_->cmd_bits;
  cmd_driver_->cmd_ready << top_->cmd_ready;
  cmd_driver_->preload_done << spad_preload_->done;

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    top_->spad_preload_val[bank]  << spad_preload_->dmaread_val[bank];
    top_->spad_preload_bits[bank] << spad_preload_->dmaread_bits[bank];
    spad_preload_->dmaread_rdy[bank] << top_->spad_preload_rdy[bank];
  }

  // Smesh's Load domain expects a real memory boundary to exist even
  // though no test here issues an Mvin through it -- left idle.
  mem_->in_core_req << top_->memReq();
  top_->memResp() << mem_->out_core_resp;
  mem_->in_core_req.setDelay(1);
  dram_->s_req << mem_->s_req;
  mem_->s_resp << dram_->s_resp;

  completion_observer_->completed_val  << top_->exCtrlCompletedVal();
  completion_observer_->completed_bits << top_->exCtrlCompletedBits();

  cmd_driver_->clk << clk;
  spad_preload_->clk << clk;
  completion_observer_->clk << clk;
  top_->clk << clk;
  mem_->clk << clk;
  dram_->clk << clk;
}

RsMem2HarnessInstance::~RsMem2HarnessInstance() = default;

void RsMem2HarnessInstance::sampleBankConcurrency() {
  std::size_t fired = 0;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (top_->spadReadReqVal(bank) != 0 && top_->spadReadReqRdy(bank) != 0) {
      ++fired;
    }
  }
  max_concurrent_spad_banks_ = std::max(max_concurrent_spad_banks_, fired);
}

bool RsMem2HarnessInstance::activityComplete() const {
  return cmd_driver_->done() && top_->rs().empty();
}

bool RsMem2HarnessInstance::passed() const {
  auto expected_tags = test_.expected_completion_tags;
  auto observed_tags = completion_observer_->observed();
  std::sort(expected_tags.begin(), expected_tags.end());
  std::sort(observed_tags.begin(), observed_tags.end());
  if (observed_tags != expected_tags) {
    return false;
  }
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
  return true;
}

void RsMem2HarnessInstance::report() const {
  std::printf("  rs_empty=%u cmd_driver_done=%u\n",
              top_->rs().empty() ? 1u : 0u, cmd_driver_->done() ? 1u : 0u);
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
}

} // namespace tb
} // namespace smesh
