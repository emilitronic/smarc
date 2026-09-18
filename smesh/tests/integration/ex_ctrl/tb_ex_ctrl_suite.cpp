// **********************************************************************
// smesh/tests/integration/ex_ctrl/tb_ex_ctrl_suite.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026
/*
Universal ExCtrl end-to-end scenario runner.  Selects scenarios, creates harness 
instances, clocks the simulation, and reports PASS/FAIL.

tb_ex_ctrl_suite.cpp
|
| selects tests (ExCtrlTestCase from tb_ex_ctrl_test_cases.cpp)
| runs one selected test in a fresh process
|
|--> ExCtrlHarnessInstance (tb_ex_ctrl_harness.hpp)
|    |
|    ExCtrlSuiteDriver
|    |
|    | SmeshIssue commands
|    V
|    ExCtrl
|    |
|    +-- SPAD read reqs --+
|    |                    |
|    |                    ExCtrlSuiteSpad (tb_ex_ctrl_harness.hpp)
|    |                    |
|    +-- SPAD read resps -+
|    |
|    +-- ACCUM writes ---------> Driver/checker
|    |
|    +-- completions ----------> Driver/checker
|    |
|    +-- Mesher observations --> Driver/checker
|
+---- result checking

Build this test runner (from smarc root):
  cmake --build build --target tb_ex_ctrl_suite -j >/dev/null 2>&1

Run one case directly (useful when tracing):
  ./build/smesh/tb_ex_ctrl_suite -list_tests
  ./build/smesh/tb_ex_ctrl_suite -test=basic
  ./build/smesh/tb_ex_ctrl_suite -test=mul_pre -trace '*'/ex_ctrl_suite_

Run cases through CTest (each case gets a fresh process):
  # List all registered tests without running them.
  ctest --test-dir build -N
  # Run one exact test.
  ctest --test-dir build -R '^smesh_ex_ctrl_basic$' --output-on-failure
  # Run a selected subset.
  ctest --test-dir build -R '^smesh_ex_ctrl_(basic|mul_pre)$' --output-on-failure
  # Run all ExCtrl tests, using up to two parallel processes (in unit + ex_ctrl).
  ctest --test-dir build -L ex_ctrl -j 2 --output-on-failure
  # Run all integration tests.
  ctest --test-dir build -L integration --output-on-failure
  # All unit tests
  ctest --test-dir build -L unit
  # All ExCtrl tests, both unit and integration
  ctest --test-dir build -L ex_ctrl
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "tb_ex_ctrl_harness.hpp"
#include "tb_ex_ctrl_test_cases.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

StringParameter(test, "basic", "One ExCtrl test name or id");
BoolParameter(list_tests, false, "List ExCtrl end-to-end tests and exit");

namespace {
// A Cascade tracer that prefixes each line with a fixed-width timestamp.
class FixedWidthCascadeTracer : public descore::Tracer {
 public:
  void traceHeader(const std::string& context, const std::string& keyname) override {
    appendTrace("[%02llu.%03llu] ",
                static_cast<unsigned long long>(Sim::simTime / 1000),
                static_cast<unsigned long long>(Sim::simTime % 1000));
    descore::Tracer::traceHeader(context, keyname);
  }

  bool traceEnabled() const override {
    return Sim::tracing;
  }
};
// Convert a component name into a prefix suitable for trace keys and log messages.
std::string componentPrefix(const std::string& name) {
  std::string prefix;
  prefix.reserve(name.size() + 1);
  for (const char ch : name) {
    prefix.push_back(std::isalnum(static_cast<unsigned char>(ch))
                         ? ch
                         : '_');
  }
  prefix.push_back('_');
  return prefix;
}
// Find one test by its command-line name or number.
const smesh::tb::ExCtrlTestCase* selectTest(
    const std::vector<smesh::tb::ExCtrlTestCase>& tests,
    const std::string& selection) {
  const auto found = std::find_if(
      tests.begin(), tests.end(), [&selection](const auto& item) {
        return item.name == selection || std::to_string(item.id) == selection;
      });
  if (found == tests.end()) {
    return nullptr;
  }
  return &*found;
}

} // namespace

int main(int argc, char* argv[]) {
  // Read command-line trace, test-selection, and waveform options.
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  // Get the list of available tests and print it if requested.
  const auto tests = smesh::tb::exCtrlTestCases();
  if (list_tests) {
    for (const auto& item : tests) {
      std::printf("%d  %-12s %s\n", item.id, item.name.c_str(), item.description.c_str());
    }
    return 0;
  }

  // Choose the one test requested on the command line.
  const auto* selected = selectTest(tests, std::string(test));
  if (selected == nullptr) {
    std::fprintf(stderr, "Unknown ExCtrl test '%s'.\n", std::string(test).c_str());
    std::fprintf(stderr, "Use -list_tests to see available tests.\n");
    return 2;
  }

  // Create one ExCtrl, driver/checker, and SPAD model for the selected test.
  const auto& selected_test = *selected;
  Clock clk;
  smesh::tb::ExCtrlHarnessInstance harness(selected_test, componentPrefix(selected_test.name), clk);
  clk.generateClock();

  // Initialize and reset the complete Cascade simulation.
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  FixedWidthCascadeTracer fixed_width_tracer;
  auto* previous_tracer = descore::setTracer(&fixed_width_tracer);
  Sim::reset();

  // Run until the test has completed and remained quiet while draining.
  int drain_count = 0;
  for (int cycle = 0; cycle < selected_test.max_cycles; ++cycle) {
    Sim::run();
    if (harness.activityComplete()) {
      ++drain_count;
    } else {
      drain_count = 0;
    }
    if (drain_count >= selected_test.drain_cycles) {
      break;
    }
  }

  // Report the selected test.
  const bool passed = harness.passed();
  std::printf("[EX_CTRL_SUITE] %s %s\n",
              passed ? "PASS" : "FAIL", selected_test.name.c_str());
  if (!passed) {
    harness.report();
  }

  // Flush buffered traces and restore the process's previous tracer.
  descore::flushLog();
  descore::setTracer(previous_tracer);
  return passed ? 0 : 1;
}
