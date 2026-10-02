// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_test_cases.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
/*
Declarative rs_to_mem test scenarios: raw SmeshCmd programs (as a host
would actually issue them), the initial scratchpad image they need, and
the accumulator result(s) they're expected to produce. See
tb_rs_to_mem_test_cases.cpp for the scenarios themselves and what each one
is exercising.

This intentionally does not carry ExCtrl-internal expectations (mesh
requests/inputs/responses, per-bank read sequences) the way ex_ctrl's own
ExCtrlTestCase does. It checks RS-assigned completions and final Accum
values; the LOOP_WS case also checks loop-slot release.
*/
#pragma once

#include "SmeshCommand.hpp"
#include "SmeshPorts.hpp"

#include <string>
#include <vector>

namespace smesh {
namespace tb {

// One initial scratchpad row: full address plus its contents. The address
// is a real, already-encoded SmeshLocalAddr (as makeSpAddr() produces),
// not a bank/row pair, since that's what Spad's write port expects.
struct SpadPreloadRow {
  SmeshLocalAddr laddr;
  MeshInputRow data;
};

// One expected C = A*B + D result: where it should land in Accum, and
// what each of its rows should contain.
struct ExpectedAccumResult {
  SmeshLocalAddr base;
  std::vector<MeshAccumRow> rows;
};

// Bytes placed in or expected from the simulated DRAM.
struct DramBytes {
  std::uint64_t addr = 0;
  std::vector<std::uint8_t> bytes;
};

struct RsMemTestCase {
  std::string name;
  std::string description;
  std::vector<SmeshCmd> program;
  std::vector<SpadPreloadRow> spad_rows;
  std::vector<DramBytes> dram_initial;
  std::vector<DramBytes> expected_dram;
  std::vector<SpadPreloadRow> expected_loaded_spad_rows;
  std::vector<ExpectedAccumResult> expected_results;
  std::vector<SmeshRsTag> expected_completion_tags;
  bool posted_writes = false;
  bool expect_loop_release = false;
  int mvin_scale_latency = 1;
  int max_cycles = 200;
  int drain_cycles = 8;

  // Spad-bank-concurrency assertions, checked against the real per-bank
  // read accept (val && rdy) signal observed across the whole run. 0 means
  // "not checked" for either field -- most test cases leave these alone.
  std::size_t min_concurrent_spad_banks = 0; // require some cycle to fire at least this many banks at once
  std::size_t max_concurrent_spad_banks = 0; // require no cycle to ever fire more than this many banks at once
};

RsMemTestCase makeBasicCase();
RsMemTestCase makeMulPreCase();
RsMemTestCase makeConcurrentBanksCase();
RsMemTestCase makeSameBankSerializesCase();
RsMemTestCase makeLoopWsCase();
RsMemTestCase makeLoopWsDmaCase();
RsMemTestCase makeLoopWsDmaI2Case();
RsMemTestCase makeLoopWsDmaK2Case();
RsMemTestCase makeFullWidthAccumLoadCase();
RsMemTestCase makeDim8LoadsCase();
std::vector<RsMemTestCase> rsMemTestCases();

} // namespace tb
} // namespace smesh
