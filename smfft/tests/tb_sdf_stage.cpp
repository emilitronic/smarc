// **********************************************************************
// smfft/tests/tb_sdf_stage.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Cycle-by-cycle check of SdfStage against the golden model.

Expected outputs are snapshots of SdfStage.step() in python/sdf_fixed.py --
the authoritative model for this block -- rather than hand arithmetic, because
the whole point of the stage is to reproduce it exactly. Regenerate with:

    cd smfft/python && python3 -c "
    import sdf_fixed as S
    p = S.Params(N=8, B_in=13, B_w=16); st = S.SdfStage(4, p)
    print([st.step(x) for x in [...]])"

Two delays are driven. d=4 runs two full blocks of 2d steps, which is what it
takes to see anything interesting: the first block only fills the line (output
is the reset zeros) and emits sums, and the stored differences do not come back
out rotated until the second block's fill half. d=1 is the degenerate stage at
the end of the chain, where every cycle alternates fill and butterfly and the
rotator is never used.

The third case is the freeze: en low for two cycles with cntr held, then the
original script resumed. The outputs after the gap must match what they would
have been without it, which is the property the chain relies on when an input
is not accepted.
*/

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "SdfStage.hpp"

#include <cstdio>
#include <vector>

namespace {

using namespace smfft;

struct Step {
  SdfComplex in;
  SdfComplex expect_out;
  int        en = 1; // 0 freezes the stage; cntr is held too
};

// d = 4, B_w = 16. Counter runs 0..7 twice.
const std::vector<Step> kScriptD4 = {
    {{100, -50}, {0, 0}},        // c=0  fill, line still holds reset zeros
    {{200, 60}, {0, 0}},         // c=1  fill, would rotate but the word is zero
    {{-300, 70}, {0, 0}},        // c=2
    {{400, -80}, {0, 0}},        // c=3
    {{500, 90}, {600, 40}},      // c=4  butterfly: sum of x0 and x4
    {{-600, 100}, {-400, 160}},  // c=5
    {{700, -110}, {400, -40}},   // c=6
    {{800, 120}, {1200, 40}},    // c=7
    {{1, 2}, {-400, -140}},      // c=0  fill: stored difference, rotator bypassed
    {{3, 4}, {537, -594}},       // c=1  stored difference rotated by W^1
    {{5, 6}, {180, 1000}},       // c=2  rotated by -j
    {{7, 8}, {141, 424}},        // c=3
    {{-9, -10}, {-8, -8}},       // c=4
    {{-11, -12}, {-8, -8}},      // c=5
    {{-13, -14}, {-8, -8}},      // c=6
    {{-15, -16}, {-8, -8}},      // c=7
};

// d = 1: no rotation is ever applied, c alternates 0 and 1.
const std::vector<Step> kScriptD1 = {
    {{10, 20}, {0, 0}},       // c=0
    {{30, 40}, {40, 60}},     // c=1
    {{-50, 60}, {-20, -20}},  // c=0
    {{70, -80}, {20, -20}},   // c=1
    {{90, 100}, {-120, 140}}, // c=0
    {{-110, 120}, {-20, 220}},// c=1
};

// Same stage, same expectations, but stalled for two cycles in the middle of
// the second block. The inputs offered while frozen are deliberately garbage
// that would corrupt the result if captured.
std::vector<Step> makeFrozenScript() {
  std::vector<Step> s;
  for (std::size_t i = 0; i < kScriptD4.size(); ++i) {
    if (i == 10) {
      s.push_back({{99999, -99999}, {180, 1000}, 0}); // frozen: output holds
      s.push_back({{77777, -77777}, {180, 1000}, 0});
    }
    s.push_back(kScriptD4[i]);
  }
  return s;
}

class StageDriver : public Component {
  DECLARE_COMPONENT(StageDriver);

 public:
  StageDriver(const std::vector<Step>& script, std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(SdfComplex, in);
  Output(u32,        cntr);
  Output(bit,        en);
  Input(SdfComplex,  out);

  void updateDrive();
  void updateCheck();
  void reset();

  bool        done() const { return step_ >= script_.size(); }
  bool        passed() const { return passed_; }
  std::size_t checked() const { return checked_; }

 private:
  const std::vector<Step>& script_;
  std::size_t step_    = 0;
  std::size_t count_   = 0; // the stage counter, advanced only when enabled
  std::size_t checked_ = 0;
  bool        passed_  = true;
};

StageDriver::StageDriver(const std::vector<Step>& script, std::string /*name*/, IMPL_CTOR)
    : script_(script) {
  UPDATE(updateDrive).writes(in, cntr, en);
  UPDATE(updateCheck).reads(out);
}

void StageDriver::updateDrive() {
  if (done()) {
    in   = SdfComplex{};
    cntr = u32(count_);
    en   = 0;
    return;
  }
  in   = script_[step_].in;
  cntr = u32(count_);
  en   = bit(script_[step_].en);
}

void StageDriver::updateCheck() {
  if (Sim::state == Sim::SimResetting || done()) {
    return;
  }
  const auto& expect = script_[step_].expect_out;
  const auto  actual = *out;
  if (actual != expect) {
    passed_ = false;
    std::printf("  step %zu (c=%zu en=%d): expected {%d,%d} got {%d,%d}\n", step_,
                count_, script_[step_].en, expect.re, expect.im, actual.re, actual.im);
  }
  ++checked_;
  // A frozen cycle advances neither the stage's state nor its counter.
  if (script_[step_].en != 0) {
    ++count_;
  }
  ++step_;
}

void StageDriver::reset() {
  step_    = 0;
  count_   = 0;
  checked_ = 0;
  passed_  = true;
  in.reset(SdfComplex{});
  cntr.reset(0);
  en.reset(0);
}

struct Harness {
  SdfStage*    stage = nullptr;
  StageDriver* drv   = nullptr;
};

Harness build(const std::vector<Step>& script, std::size_t d, Clock& clk) {
  Harness h;
  h.stage = new SdfStage("Stage", d, 16);
  h.drv   = new StageDriver(script, "Driver");

  h.stage->in   << h.drv->in;
  h.stage->cntr << h.drv->cntr;
  h.stage->en   << h.drv->en;
  h.drv->out    << h.stage->out;

  h.stage->clk << clk;
  h.drv->clk   << clk;
  return h;
}

bool report(const char* what, const Harness& h) {
  const bool ok = h.drv->passed() && h.drv->done();
  if (!ok) {
    std::printf("  %s: checked=%zu passed=%d done=%d\n", what, h.drv->checked(),
                h.drv->passed() ? 1 : 0, h.drv->done() ? 1 : 0);
  }
  std::printf("  [%s] %s, %zu cycles\n", ok ? "PASS" : "FAIL", what, h.drv->checked());
  return ok;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  const std::vector<Step> frozen = makeFrozenScript();

  Clock clk;
  const Harness h4 = build(kScriptD4, 4, clk);
  const Harness h1 = build(kScriptD1, 1, clk);
  const Harness hf = build(frozen, 4, clk);
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();

  std::size_t cycles = kScriptD4.size();
  cycles = kScriptD1.size() > cycles ? kScriptD1.size() : cycles;
  cycles = frozen.size() > cycles ? frozen.size() : cycles;
  for (std::size_t c = 0; c < cycles; ++c) {
    Sim::run();
  }

  bool ok = true;
  ok &= report("d=4, two blocks", h4);
  ok &= report("d=1, degenerate", h1);
  ok &= report("d=4, two-cycle freeze", hf);

  std::printf("[SDF_STAGE] %s\n", ok ? "PASS" : "FAIL");
  descore::flushLog();
  return ok ? 0 : 1;
}
