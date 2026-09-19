// **********************************************************************
// smesh/tests/integration/rs_to_mem/tb_rs_to_mem_suite.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
/*
rs_to_mem (ExCtrl slice) scenario runner. Selects a scenario, builds one
RsMemHarnessInstance, clocks the simulation, and reports PASS/FAIL. See
README.md for scope and tb_rs_to_mem_harness.hpp for what's being wired.

Run:
  cmake --build build --target tb_rs_to_mem_suite -j
  ./build/smesh/tb_rs_to_mem_suite -test=basic
  ./build/smesh/tb_rs_to_mem_suite -test=mul_pre
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "tb_rs_to_mem_harness.hpp"
#include "tb_rs_to_mem_test_cases.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

StringParameter(test, "basic", "One rs_to_mem test name");
BoolParameter(list_tests, false, "List rs_to_mem tests and exit");

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  const auto tests = smesh::tb::rsMemTestCases();
  if (list_tests) {
    for (const auto& t : tests) {
      std::printf("%-10s %s\n", t.name.c_str(), t.description.c_str());
    }
    return 0;
  }

  const auto found = std::find_if(tests.begin(), tests.end(),
                                  [](const smesh::tb::RsMemTestCase& t) {
                                    return t.name == std::string(test);
                                  });
  if (found == tests.end()) {
    std::fprintf(stderr, "Unknown rs_to_mem test '%s'.\n", std::string(test).c_str());
    return 2;
  }
  const auto& tc = *found;

  Clock clk;
  smesh::tb::RsMemHarnessInstance harness(tc, "RsMem_", clk);
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();

  int drain_count = 0;
  for (int cycle = 0; cycle < tc.max_cycles; ++cycle) {
    Sim::run();
    drain_count = harness.activityComplete() ? drain_count + 1 : 0;
    if (drain_count >= tc.drain_cycles) {
      break;
    }
  }

  const bool passed = harness.passed();
  std::printf("[RS_TO_MEM_SUITE] %s %s\n", passed ? "PASS" : "FAIL", tc.name.c_str());
  if (!passed) {
    harness.report();
  }
  return passed ? 0 : 1;
}
