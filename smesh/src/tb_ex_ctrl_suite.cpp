// **********************************************************************
// smesh/src/tb_ex_ctrl_suite.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 17 2026
/*
Universal ExCtrl end-to-end scenario runner.  Selects scenarios, creates harness 
instances, clocks the simulation, and reports PASS/FAIL.

tb_ex_ctrl_suite.cpp
|
| selects tests (ExCtrlTestCase from tb_ex_ctrl_test_cases.cpp)
| creates one ExCtrlHarnessInstance for each selected test case
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

cmake --build build --target tb_ex_ctrl_suite -j >/dev/null 2>&1
./build/smesh/tb_ex_ctrl_suite -list_tests=1
./build/smesh/tb_ex_ctrl_suite -test=basic
./build/smesh/tb_ex_ctrl_suite -test=mul_pre
./build/smesh/tb_ex_ctrl_suite -test=1,2
./build/smesh/tb_ex_ctrl_suite -test=all
./build/smesh/tb_ex_ctrl_suite -test=mul_pre -trace '*'/ex_ctrl_suite_
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "tb_ex_ctrl_harness.hpp"
#include "tb_ex_ctrl_test_cases.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

StringParameter(test, "all", "ExCtrl test name/id, comma-separated list, or all");
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
// Select a subset of tests based on the command-line selection string.
std::vector<const smesh::tb::ExCtrlTestCase*> selectTests(
    const std::vector<smesh::tb::ExCtrlTestCase>& tests,
    const std::string& selection) {
  if (selection == "all") {
    std::vector<const smesh::tb::ExCtrlTestCase*> selected;
    for (const auto& item : tests) {
      selected.push_back(&item);
    }
    return selected;
  }

  std::vector<const smesh::tb::ExCtrlTestCase*> selected;
  std::stringstream stream(selection);
  std::string token;
  while (std::getline(stream, token, ',')) {
    const auto found = std::find_if(
        tests.begin(), tests.end(), [&token](const auto& item) {
          return item.name == token || std::to_string(item.id) == token;
        });
    if (found == tests.end()) {
      std::fprintf(stderr, "Unknown ExCtrl test '%s'\n", token.c_str());
      return {};
    }
    selected.push_back(&*found);
  }
  return selected;
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

  // Choose which tests to run from the command-line setting.
  const auto selected = selectTests(tests, std::string(test));
  if (selected.empty()) {
    std::fprintf(stderr, "Use -list_tests=1 to see available tests.\n");
    return 2;
  }

  // Give each selected case its own ExCtrl, driver/checker, and SPAD model.
  Clock clk;
  // create list of test harnesses, one for each selected test case
  std::vector<std::unique_ptr<smesh::tb::ExCtrlHarnessInstance>> harnesses;
  // visit every selected test case and create a harness for it
  for (const auto* item : selected) {
    // harness contructor receives: completed test description, name prefix, shared simulaiton clock
    harnesses.emplace_back(new smesh::tb::ExCtrlHarnessInstance(*item, componentPrefix(item->name), clk));
  }
  clk.generateClock();

  // Selected tests currently run as independent harnesses in one Cascade
  // simulation. This avoids assuming that Cascade can discard all global
  // simulation state and safely call Sim::init() again in the same process.
  // TODO: determine whether each test can instead run in a fresh simulation.
  // Separate processes would also allow suites to run sequentially or, when
  // useful, in parallel without sharing Cascade simulation state.

  // Initialize and reset the complete Cascade simulation.
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  FixedWidthCascadeTracer fixed_width_tracer;
  auto* previous_tracer = descore::setTracer(&fixed_width_tracer);
  Sim::reset();

  // Run until every case has completed and remained quiet while draining.
  int max_cycles = 0;
  for (const auto* item : selected) {
    max_cycles = std::max(max_cycles, item->max_cycles); // find largest permitted runtime amont selected test cases
  }
  std::vector<int> drain_count(selected.size(), 0); // one drain counter per test (records how many consecutive cycles a test has appered complete)
  for (int cycle = 0; cycle < max_cycles; ++cycle) {
    Sim::run();
    bool all_drained = true;
    for (std::size_t i = 0; i < harnesses.size(); ++i) {
      if (harnesses[i]->activityComplete()) {
        ++drain_count[i];
      } else {
        drain_count[i] = 0;
      }
      all_drained &= drain_count[i] >= selected[i]->drain_cycles;
    }
    if (all_drained) {
      break;
    }
  }

  // Report each selected case and return failure if any case failed.
  bool all_passed = true;
  for (std::size_t i = 0; i < harnesses.size(); ++i) {
    const bool passed = harnesses[i]->passed();
    std::printf("[EX_CTRL_SUITE] %s %s\n",
                passed ? "PASS" : "FAIL", selected[i]->name.c_str());
    if (!passed) {
      harnesses[i]->report();
    }
    all_passed &= passed;
  }

  // Flush buffered traces and restore the process's previous tracer.
  descore::flushLog();
  descore::setTracer(previous_tracer);
  return all_passed ? 0 : 1;
}
