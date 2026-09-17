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
| runs one test directly, or launches one fresh process per selected test
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
./build/smesh/tb_ex_ctrl_suite -test=2,1
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
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
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

// Run one selected test in a fresh copy of this executable.
int runTestProcess(const std::vector<std::string>& command_line,
                   const std::string& test_name) {
  std::vector<std::string> arguments;
  arguments.push_back(command_line.front());
  for (std::size_t i = 1; i < command_line.size(); ++i) {
    const auto& argument = command_line[i];
    if (argument == "-test") {
      ++i;
      continue;
    }
    if (argument.rfind("-test=", 0) == 0) {
      continue;
    }
    arguments.push_back(argument);
  }
  arguments.push_back("-test=" + test_name);

  std::vector<char*> child_argv;
  for (auto& argument : arguments) {
    child_argv.push_back(&argument[0]);
  }
  child_argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid == 0) {
    execvp(child_argv[0], child_argv.data());
    std::perror("execvp");
    _exit(127);
  }
  if (pid < 0) {
    std::perror("fork");
    return 1;
  }

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    std::perror("waitpid");
    return 1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

// Run multiple selected tests one at a time, each with fresh Cascade state.
int runTestProcesses(const std::vector<std::string>& command_line,
                     const std::vector<const smesh::tb::ExCtrlTestCase*>& selected) {
  bool all_passed = true;
  for (const auto* item : selected) {
    std::printf("[EX_CTRL_SUITE] RUN  %s\n", item->name.c_str());
    std::fflush(stdout);
    all_passed &= runTestProcess(command_line, item->name) == 0;
  }
  return all_passed ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
  // Preserve options because Cascade's parsers may modify argc/argv.
  std::vector<std::string> command_line;
  for (int i = 0; i < argc; ++i) {
    command_line.emplace_back(argv[i]);
  }

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

  // Multiple tests run as separate processes so each gets fresh Cascade state.
  if (selected.size() > 1) {
    return runTestProcesses(command_line, selected);
  }

  // Create one ExCtrl, driver/checker, and SPAD model for the selected test.
  const auto& selected_test = *selected.front();
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
