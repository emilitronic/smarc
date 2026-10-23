// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_harness.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026

#include "tb_rs_to_mem_harness.hpp"
#include "tb_rs_to_mem_monitor.hpp"

#include <algorithm>
#include <cstdio>

namespace smesh {
namespace tb {

RsMemCmdDriver::RsMemCmdDriver(const std::vector<SmeshCmd>& program, std::string /*name*/, IMPL_CTOR)
    : program_(program) {
  UPDATE(update).reads(cmd_ready).writes(cmd_valid, cmd_bits);
}

void RsMemCmdDriver::update() {
  cmd_valid = 0;
  cmd_bits  = SmeshCmd{};
  if (Sim::state == Sim::SimResetting || done()) {
    return;
  }
  cmd_valid = 1;
  cmd_bits  = program_[next_];
  if (cmd_ready == 1) {
    ++next_;
  }
}

void RsMemCmdDriver::reset() {
  next_ = 0;
  cmd_valid.reset(0);
  cmd_bits.reset(SmeshCmd{});
}

RsMemHarnessInstance::RsMemHarnessInstance(const RsMemTestCase& test, const std::string& prefix, Clock& clk)
    : test_(test) {
  top_ = std::make_unique<Smesh>(prefix + "Smesh", test_.mvin_scale_latency);
  mem_ = std::make_unique<smem::MemCtrl>(prefix + "MemCtrl");
  mem_->set_posted_writes(test_.posted_writes);
  dram_ = std::make_unique<smem::Dram>(prefix + "Dram", 0);

  cmd_driver_ = std::make_unique<RsMemCmdDriver>(test_.program, prefix + "CmdDriver");

  top_->cmd_valid << cmd_driver_->cmd_valid;
  top_->cmd_bits  << cmd_driver_->cmd_bits;
  cmd_driver_->cmd_ready << top_->cmd_ready;

  // Connect the DMA reader and writer to the shared simulated DRAM.
  mem_->in_core_req << top_->memReq();
  top_->memResp() << mem_->out_core_resp;
  mem_->in_core_req.setDelay(1);
  dram_->s_req << mem_->s_req;
  mem_->s_resp << dram_->s_resp;

  cmd_driver_->clk << clk;
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

bool RsMemHarnessInstance::inputCommandAccepted() const {
  return cmd_driver_->cmd_valid == 1 && cmd_driver_->cmd_ready == 1;
}

bool RsMemHarnessInstance::activityComplete() const {
  if (!cmd_driver_->done() || !top_->rs().empty()) return false;
  // Store completion can precede the scaler and writer finishing their rows.
  // Start the final drain countdown only after the expected DRAM writes appear.
  if (!dramResultsMatch() || !mem_->writes_empty()) return false;
  if (test_.expect_conv_release &&
      (top_->loopConv().busy == 1 || top_->loopConv().ld_outstanding != 0 ||
       top_->loopConv().ex_outstanding != 0 || top_->loopConv().st_outstanding != 0)) return false;
  return !test_.expect_loop_release ||
      (top_->loopMatmul().loop0->configured == 0 &&
       top_->loopMatmul().loop1->configured == 0);
}

bool RsMemHarnessInstance::passed(const RsMemMonitor& monitor) const {
  if (!convExecuteMatches(monitor)) return false;
  auto expected_tags = test_.expected_completion_tags;
  auto observed_tags = monitor.completions();
  std::sort(expected_tags.begin(), expected_tags.end());
  std::sort(observed_tags.begin(), observed_tags.end());
  if (observed_tags != expected_tags) {
    return false;
  }
  if (test_.expect_loop_release &&
      (top_->loopMatmul().loop0->configured == 1 ||
       top_->loopMatmul().loop1->configured == 1)) return false;
  if (test_.expect_conv_release &&
      (top_->loopConv().busy == 1 || top_->loopConv().loop0->configured == 1 ||
       top_->loopConv().loop1->configured == 1 || top_->loopConv().ld_outstanding != 0 ||
       top_->loopConv().ex_outstanding != 0 || top_->loopConv().st_outstanding != 0)) return false;
  if (test_.min_concurrent_spad_banks > 0 &&
      monitor.maxConcurrentSpadBanks() < test_.min_concurrent_spad_banks) {
    return false;
  }
  if (test_.max_concurrent_spad_banks > 0 &&
      monitor.maxConcurrentSpadBanks() > test_.max_concurrent_spad_banks) {
    return false;
  }
  if (monitor.exAccumReads() < test_.min_ex_accum_reads ||
      monitor.storeAccumReads() < test_.min_store_accum_reads ||
      (test_.require_concurrent_accum_reads && !monitor.concurrentAccumReads())) return false;
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
  return dramResultsMatch();
}

bool RsMemHarnessInstance::dramResultsMatch() const {
  for (const auto& expected : test_.expected_dram) {
    std::vector<std::uint8_t> actual(expected.bytes.size());
    dram_->read(expected.addr, actual.data(), actual.size());
    if (actual != expected.bytes) return false;
  }
  return true;
}

bool RsMemHarnessInstance::convExecuteMatches(const RsMemMonitor& monitor) const {
  if (test_.expected_conv_execute.empty()) return true;
  const auto& observed = monitor.convExecuteCommands();
  if (observed.size() != test_.expected_conv_execute.size()) return false;
  for (std::size_t i = 0; i < observed.size(); ++i) {
    const auto& expected = test_.expected_conv_execute[i];
    const auto& actual = observed[i];
    if (actual.funct != expected.funct || actual.rs1 != expected.rs1 || actual.rs2 != expected.rs2)
      return false;
  }
  return true;
}

void RsMemHarnessInstance::report(const RsMemMonitor& monitor) const {
  if (!convExecuteMatches(monitor)) {
    std::printf("  LoopConv execute commands: expected=%zu observed=%zu\n",
                test_.expected_conv_execute.size(), monitor.convExecuteCommands().size());
    for (const auto& cmd : monitor.convExecuteCommands())
      std::printf("    funct=%u rs1=0x%llx rs2=0x%llx\n", unsigned(cmd.funct),
                  static_cast<unsigned long long>(cmd.rs1), static_cast<unsigned long long>(cmd.rs2));
  }
  std::printf("  rs_empty=%u cmd_driver_done=%u\n",
              top_->rs().empty() ? 1u : 0u, cmd_driver_->done() ? 1u : 0u);
  if (test_.expect_conv_release) {
    std::printf("  loop_conv busy=%u slots={%u,%u} outstanding={%u,%u,%u}\n",
        unsigned(top_->loopConv().busy), unsigned(top_->loopConv().loop0->configured),
        unsigned(top_->loopConv().loop1->configured), unsigned(top_->loopConv().ld_outstanding),
        unsigned(top_->loopConv().ex_outstanding), unsigned(top_->loopConv().st_outstanding));
  }
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
  for (const auto tag : monitor.completions()) {
    std::printf(" %u", static_cast<unsigned>(tag));
  }
  std::printf("\n");
  std::printf("  max_concurrent_spad_banks observed=%zu expected_min=%zu expected_max=%zu\n",
              monitor.maxConcurrentSpadBanks(), test_.min_concurrent_spad_banks,
              test_.max_concurrent_spad_banks);
  std::printf("  accum reads ex=%zu store=%zu concurrent=%u\n",
              monitor.exAccumReads(), monitor.storeAccumReads(), monitor.concurrentAccumReads() ? 1u : 0u);
  std::printf("  ExCtrl accumulator response stalled cycles=%zu\n",
              monitor.exAccumResponseStallCycles());
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
      std::printf("  spad[%u] expected={", expected.laddr.data());
      for (std::size_t lane = 0; lane < kDim; ++lane)
        std::printf("%s%d", lane == 0 ? "" : ",", int(expected.data[lane]));
      std::printf("} actual={");
      for (std::size_t lane = 0; lane < kDim; ++lane)
        std::printf("%s%d", lane == 0 ? "" : ",", int(actual[lane]));
      std::printf("}\n");
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
