// **********************************************************************
// smfft/tests/tb_sdf_delay_line.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 10 2026
/*
Standalone check of SdfDelayLine: d-cycle delay, zeros before the line
fills, and a complete freeze while en is low (the input presented during a
frozen cycle is dropped, which is what "no accepted input, no state
change" means for the chain).

Expected outputs are written out by hand rather than computed, so the table
itself is the specification of the behaviour.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SdfDelayLine.hpp"

#include <cstdio>
#include <vector>

namespace {

using namespace smfft;

struct Step {
  SdfComplex in;
  int en;
  SdfComplex expect_out;
};

// depth 4: fill, freeze for two cycles, then drain
const std::vector<Step> kScriptD4 = {
    {{1, 101}, 1, {0, 0}},     // 0  line empty
    {{2, 102}, 1, {0, 0}},     // 1
    {{3, 103}, 1, {0, 0}},     // 2
    {{4, 104}, 1, {0, 0}},     // 3
    {{9, 109}, 0, {1, 101}},   // 4  frozen: holds, drops the input
    {{9, 109}, 0, {1, 101}},   // 5  still frozen, same word held
    {{5, 105}, 1, {1, 101}},   // 6  enabled: emits in0, captures 5
    {{6, 106}, 1, {2, 102}},   // 7
    {{7, 107}, 1, {3, 103}},   // 8
    {{8, 108}, 1, {4, 104}},   // 9
    {{0, 0}, 1, {5, 105}},     // 10 the word captured at step 6
    {{0, 0}, 1, {6, 106}},     // 11
};

// depth 1: the degenerate case, one cycle of delay
const std::vector<Step> kScriptD1 = {
    {{7, 7}, 1, {0, 0}}, // 0
    {{8, 8}, 1, {7, 7}}, // 1
    {{9, 9}, 1, {8, 8}}, // 2
    {{0, 0}, 0, {9, 9}}, // 3 frozen
    {{1, 1}, 1, {9, 9}}, // 4 enabled: emits 9, captures 1
    {{0, 0}, 1, {1, 1}}, // 5
};

class DelayLineDriver : public Component {
  DECLARE_COMPONENT(DelayLineDriver);

 public:
  DelayLineDriver(const std::vector<Step>& script, std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(SdfComplex, in);
  Output(bit, en);
  Input(SdfComplex, out);

  void updateDrive();
  void updateCheck();
  void reset();

  bool done() const { return step_ >= script_.size(); }
  bool passed() const { return passed_; }
  std::size_t checked() const { return checked_; }

 private:
  const std::vector<Step>& script_;
  std::size_t step_ = 0;
  std::size_t checked_ = 0;
  bool passed_ = true;
};

DelayLineDriver::DelayLineDriver(const std::vector<Step>& script, std::string /*name*/, IMPL_CTOR)
    : script_(script) {
  UPDATE(updateDrive).writes(in, en);
  UPDATE(updateCheck).reads(out);
}

void DelayLineDriver::updateDrive() {
  if (done()) {
    in = SdfComplex{};
    en = 0;
    return;
  }
  in = script_[step_].in;
  en = bit(script_[step_].en);
}

void DelayLineDriver::updateCheck() {
  if (Sim::state == Sim::SimResetting || done()) {
    return;
  }
  const auto& expect = script_[step_].expect_out;
  const auto actual = *out;
  if (actual != expect) {
    passed_ = false;
    std::printf("  step %zu: expected {%d,%d} got {%d,%d}\n", step_, expect.re, expect.im, actual.re,
                actual.im);
  }
  ++checked_;
  ++step_;
}

void DelayLineDriver::reset() {
  step_ = 0;
  checked_ = 0;
  passed_ = true;
  in.reset(SdfComplex{});
  en.reset(0);
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smfft::SdfDelayLine dl4("DelayLine4", 4);
  smfft::SdfDelayLine dl1("DelayLine1", 1);
  DelayLineDriver drv4(kScriptD4, "Driver4");
  DelayLineDriver drv1(kScriptD1, "Driver1");

  dl4.in << drv4.in;
  dl4.en << drv4.en;
  drv4.out << dl4.out;

  dl1.in << drv1.in;
  dl1.en << drv1.en;
  drv1.out << dl1.out;

  Clock clk;
  dl4.clk << clk;
  dl1.clk << clk;
  drv4.clk << clk;
  drv1.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();

  const std::size_t cycles = kScriptD4.size() > kScriptD1.size() ? kScriptD4.size() : kScriptD1.size();
  for (std::size_t c = 0; c < cycles; ++c) {
    Sim::run();
  }

  const bool ok = drv4.passed() && drv1.passed() && drv4.done() && drv1.done();
  if (!ok) {
    std::printf("  d4: checked=%zu passed=%d done=%d\n", drv4.checked(), drv4.passed() ? 1 : 0,
                drv4.done() ? 1 : 0);
    std::printf("  d1: checked=%zu passed=%d done=%d\n", drv1.checked(), drv1.passed() ? 1 : 0,
                drv1.done() ? 1 : 0);
  }
  std::printf("[SDF_DELAY_LINE] %s delay_and_freeze\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
