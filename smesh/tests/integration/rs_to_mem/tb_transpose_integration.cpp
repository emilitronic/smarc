// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_transpose_integration.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 24 2026
/*
Verify TransposePreloadUnroller inside ExCtrl using ordinary RS commands.
Check one pair, delayed arrival of COMPUTE_FLIP, and sixteen consecutive pairs
that fill ExCtrl's command queue and backpressure the unroller and RS.
Every pair must retain helper/PRELOAD overlap, produce A*B^T+D, and complete
only its two original RS tags. The helpers never enter the RS.

  cmake --build build --target tb_transpose_integration -j
  ./build/smesh/tb_transpose_integration
*/
#include "tb_rs_to_mem_harness.hpp"
#include "tb_rs_to_mem_monitor.hpp"

#include <descore/Parameter.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

namespace {
using namespace smesh;
using namespace smesh::tb;

struct RunStats {
  std::array<unsigned, kSpBanks> reads{};
  unsigned writes = 0;
  unsigned requests = 0;
  unsigned result_frames = 0;
  unsigned result_rows = 0;
  unsigned queue_stalls = 0;
  unsigned rs_stalls = 0;
  unsigned expanded_commands = 0;
  bool valid = true;

  void sample(const Smesh& top, const ExpectedAccumResult& expected) {
    const auto& ex = top.exCtrl();
    const auto* unroller = ex.preloadUnroller();
    valid &= unroller != nullptr;
    if (unroller) {
      if (unroller->out_val == 1 && unroller->out_rdy == 0) ++queue_stalls;
      if (unroller->out_val == 1 && unroller->out_rdy == 1) ++expanded_commands;
    }
    if (ex.cmd_val == 1 && ex.cmd_rdy == 0) ++rs_stalls;

    for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
      const auto& spad = top.spad();
      if (spad.read_req_val_bnk[bank] == 1 && spad.read_req_rdy_bnk[bank] == 1) ++reads[bank];
    }
    for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
      const auto& accum = top.accum();
      if (accum.write_val_bnk[bank] == 1 && accum.write_rdy_bnk[bank] == 1) {
        valid &= accum.write_bits_bnk[bank]->laddr.full_acc_addr() == writes % kDim;
        ++writes;
      }
    }

    // Successive request destinations are garbage,C,garbage,C,...,garbage.
    // Each COMPUTE_FLIP may also overlap the next pair's first PRELOAD.
    if (ex.mesher_req_val == 1 && ex.mesher_req_rdy == 1) {
      const auto request = *ex.mesher_req_bits;
      const bool keep = requests % 2 == 1;
      valid &= request.tag.addr.is_garbage() != keep;
      valid &= request.total_rows == kDim;
      valid &= (request.pe_control.propagate == 1) == (requests > 0 && !keep);
      if (keep) valid &= request.tag.rs_tag_valid == 1 && request.tag.rs_tag == requests;
      ++requests;
    }
    // Delayed tags make result destinations garbage,garbage,C,garbage,C,...,C.
    if (ex.mesher_resp_val == 1) {
      const auto response = *ex.mesher_resp_bits;
      const bool keep = result_frames >= 2 && result_frames % 2 == 0;
      valid &= response.tag.addr.is_garbage() != keep;
      if (keep) {
        const unsigned job = result_rows / kDim;
        valid &= response.tag.rs_tag_valid == 1 && response.tag.rs_tag == 1 + 2 * job;
        valid &= response.tag.addr.raw == expected.base.raw;
        valid &= response.data == expected.rows[result_rows % kDim];
        ++result_rows;
      }
      if (response.last == 1) ++result_frames;
    }
  }
};
} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  const auto catalog = rsMemTestCases();
  const auto found = std::find_if(catalog.begin(), catalog.end(), [](const RsMemTestCase& test) {
    return test.name == "transpose_b";
  });
  if (found == catalog.end()) return 2;
  auto test = *found;
  const auto pair_program = test.program;
  test.max_cycles = 4000;
  Clock clk;
  RsMemHarnessInstance harness(test, "Transpose_", clk);
  RsMemMonitor monitor(harness.smesh());
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();

  bool ok = true;
  for (unsigned trial = 0; trial < 3; ++trial) {
    const unsigned jobs = trial == 2 ? 16 : 1;
    const bool delayed = trial == 1;
    test.program = {pair_program[0]};
    for (unsigned job = 0; job < jobs; ++job) {
      test.program.push_back(pair_program[1]);
      test.program.push_back(pair_program[2]);
    }
    test.expected_completion_tags.clear();
    for (unsigned tag = 0; tag < test.program.size(); ++tag)
      test.expected_completion_tags.push_back(static_cast<SmeshRsTag>(tag));
    if (delayed) test.program.resize(2); // hold COMPUTE_FLIP outside Smesh until cycle 40.
    Sim::reset();
    monitor.reset();
    harness.initializeMemoryImages();
    RunStats stats;
    unsigned drain = 0;
    for (int cycle = 0; cycle < test.max_cycles; ++cycle) {
      if (delayed && cycle == 40) test.program.push_back(pair_program[2]);
      Sim::run();
      monitor.sample();
      stats.sample(harness.smesh(), test.expected_results[0]);
      if (delayed && cycle < 40) stats.valid &= stats.requests == 0;
      drain = harness.activityComplete() ? drain + 1 : 0;
      if (drain >= static_cast<unsigned>(test.drain_cycles)) break;
    }
    bool passed = stats.valid && harness.activityComplete() && harness.passed(monitor) &&
                  stats.requests == 2 * jobs + 1 && stats.result_frames == stats.requests &&
                  stats.writes == jobs * kDim && stats.result_rows == jobs * kDim &&
                  stats.expanded_commands == 4 * jobs + 1;
    for (std::size_t bank = 0; bank < kSpBanks; ++bank)
      passed &= stats.reads[bank] == (bank < 3 ? jobs * kDim : 0);
    if (jobs > 1) passed &= stats.queue_stalls > 0 && stats.rs_stalls > 0;
    descore::flushLog();
    std::printf("[TRANSPOSE_INTEGRATION] %s %s DIM=%zu jobs=%u requests=%u writes=%u expanded=%u queue_stalls=%u rs_stalls=%u completions=%zu\n",
                passed ? "PASS" : "FAIL", trial == 0 ? "single" : delayed ? "delayed_compute" : "queue_pressure",
                kDim, jobs, stats.requests, stats.writes, stats.expanded_commands, stats.queue_stalls,
                stats.rs_stalls, monitor.completion().observed().size());
    if (!passed) harness.report(monitor);
    ok &= passed;
  }
  descore::flushLog();
  return ok ? 0 : 1;
}
