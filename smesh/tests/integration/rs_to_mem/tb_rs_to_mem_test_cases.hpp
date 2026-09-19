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
ExCtrlTestCase does -- rs_to_mem treats ExCtrl as an already-validated,
opaque block (see README.md), so only two things are checked here: the
RS-assigned completion-tag order, and the final values ExCtrl's writeback
actually lands in Accum.
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

struct RsMemTestCase {
  std::string name;
  std::string description;
  std::vector<SmeshCmd> program;
  std::vector<SpadPreloadRow> spad_rows;
  std::vector<ExpectedAccumResult> expected_results;
  std::vector<SmeshRsTag> expected_completion_order;
  int max_cycles = 200;
  int drain_cycles = 8;
};

RsMemTestCase makeBasicCase();
RsMemTestCase makeMulPreCase();
std::vector<RsMemTestCase> rsMemTestCases();

} // namespace tb
} // namespace smesh
