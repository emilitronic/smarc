// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_transpose_helper.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 24 2026
/*
Compare real and garbage helper operands in the explicitly expanded WS
B-transpose sequence through Smesh. This does not simulate Original's RTL
or connect TransposePreloadUnroller inside ExCtrl.

The separated case initially supplies only CONFIG, PRELOAD, and helper
COMPUTE_STAY. After the helper completes, the test appends the second PRELOAD
and final COMPUTE_FLIP to the driver's program. The driver holds a reference
to that vector, so no hardware connections or functional blocks change.
It currently fails the matrix-product check with either helper: the extra
standalone PRELOAD advances the transposer again and loses the loaded weights.
Use -separate_helper to reproduce this known scheduling limitation. The default
regression checks the supported overlapping sequence, including request count.

  cmake --build build --target tb_transpose_helper -j
  ./build/smesh/tb_transpose_helper
*/
#include "tb_rs_to_mem_harness.hpp"
#include "tb_rs_to_mem_monitor.hpp"

#include <descore/Parameter.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

BoolParameter(separate_helper, false, "Also run the known-failing forced-separated helper diagnostic");

namespace {
using namespace smesh;
using namespace smesh::tb;

struct Measurements {
  std::array<unsigned, kSpBanks> reads{};
  unsigned writes = 0;
  unsigned mesh_requests = 0;
  bool write_addresses_ok = true;
  bool result_rows_ok = true;
  unsigned kept_rows = 0;
  std::vector<bool> request_kept;
  std::vector<bool> result_kept;

  void sample(const Smesh& top, const ExpectedAccumResult& expected) {
    const auto& spad = top.spad();
    for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
      if (spad.read_req_val_bnk[bank] == 1 && spad.read_req_rdy_bnk[bank] == 1)
        ++reads[bank];
    }
    const auto& accum = top.accum();
    for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
      if (accum.write_val_bnk[bank] == 1 && accum.write_rdy_bnk[bank] == 1) {
        const auto addr = accum.write_bits_bnk[bank]->laddr;
        write_addresses_ok &= addr.full_acc_addr() == writes;
        ++writes;
      }
    }
    const auto& ex = top.exCtrl();
    if (ex.mesher_req_val == 1 && ex.mesher_req_rdy == 1) {
      ++mesh_requests;
      request_kept.push_back(!ex.mesher_req_bits->tag.addr.is_garbage());
    }
    if (ex.mesher_resp_val == 1) {
      const auto response = *ex.mesher_resp_bits;
      const bool kept = !response.tag.addr.is_garbage();
      if (kept) {
        result_rows_ok &= response.tag.rs_tag_valid == 1 && response.tag.rs_tag == 3 &&
                          response.tag.addr.raw == expected.base.raw &&
                          kept_rows < expected.rows.size();
        if (kept_rows < expected.rows.size()) result_rows_ok &= response.data == expected.rows[kept_rows];
        ++kept_rows;
      }
      if (response.last == 1) result_kept.push_back(kept);
    }
  }
};
} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  const auto tests = rsMemTestCases();
  const auto found = std::find_if(tests.begin(), tests.end(), [](const RsMemTestCase& test) {
    return test.name == "transpose_b_expanded";
  });
  if (found == tests.end()) return 2;
  auto test = *found;
  const auto original_program = test.program;

  Clock clk;
  RsMemHarnessInstance harness(test, "Helper_", clk);
  RsMemMonitor monitor(harness.smesh());
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();

  if (separate_helper) {
    std::printf("[TRANSPOSE_HELPER] Running the known-failing separated-sequence diagnostic.\n");
  }
  bool ok = true;
  for (bool separate : {false, true}) {
    if (separate && !separate_helper) continue;
    for (bool real_operands : {true, false}) {
      test.program = original_program;
      if (real_operands) {
        // Reproduce the actual operand payloads retained by Original's helper.
        test.program[2].rs1 = test.program[4].rs1;
        test.program[2].rs2 = test.program[4].rs2;
      }
      if (separate) test.program.resize(3);
      Sim::reset();
      monitor.reset();
      harness.initializeMemoryImages();
      Measurements measured;
      bool released = !separate;
      unsigned drain = 0;
      for (int cycle = 0; cycle < test.max_cycles; ++cycle) {
        Sim::run();
        monitor.sample();
        measured.sample(harness.smesh(), test.expected_results[0]);
        const auto& completions = monitor.completion().observed();
        if (!released && std::find(completions.begin(), completions.end(), SmeshRsTag{2}) != completions.end()) {
          test.program.insert(test.program.end(), original_program.begin() + 3, original_program.end());
          released = true;
        }
        drain = released && harness.activityComplete() ? drain + 1 : 0;
        if (drain >= static_cast<unsigned>(test.drain_cycles)) break;
      }

      const unsigned expected_operand_reads = (real_operands ? 2 : 1) * kDim;
      const bool reads_ok = measured.reads[0] == expected_operand_reads &&
                            measured.reads[1] == kDim &&
                            measured.reads[2] == expected_operand_reads &&
                            measured.reads[3] == 0;
      const std::vector<bool> expected_requests = separate
          ? std::vector<bool>{false, false, true, false}
          : std::vector<bool>{false, true, false};
      const std::vector<bool> expected_results = separate
          ? std::vector<bool>{false, false, false, true}
          : std::vector<bool>{false, false, true};
      const bool passed = released && harness.activityComplete() && harness.passed(monitor) &&
                          reads_ok && measured.writes == kDim && measured.write_addresses_ok &&
                          measured.result_rows_ok && measured.kept_rows == kDim &&
                          measured.request_kept == expected_requests && measured.result_kept == expected_results &&
                          measured.mesh_requests == (separate ? 4u : 3u);
      descore::flushLog();
      std::printf("[TRANSPOSE_HELPER] %s %s %s DIM=%zu reads[A,B,D]=[%u,%u,%u] writes=%u mesh_requests=%u completions=%zu\n",
                  passed ? "PASS" : "FAIL", separate ? "separate" : "overlap",
                  real_operands ? "real" : "garbage", kDim,
                  measured.reads[0], measured.reads[1], measured.reads[2], measured.writes,
                  measured.mesh_requests, monitor.completion().observed().size());
      if (!passed) {
        std::printf("  reads_ok=%u write_addresses_ok=%u result_rows_ok=%u kept_rows=%u request_tags_ok=%u result_tags_ok=%u\n",
                    unsigned(reads_ok), unsigned(measured.write_addresses_ok), unsigned(measured.result_rows_ok),
                    measured.kept_rows, unsigned(measured.request_kept == expected_requests),
                    unsigned(measured.result_kept == expected_results));
        harness.report(monitor);
      }
      ok &= passed;
    }
  }
  descore::flushLog();
  return ok ? 0 : 1;
}
