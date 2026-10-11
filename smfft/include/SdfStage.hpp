// **********************************************************************
// smfft/include/SdfStage.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
One radix-2 DIF SDF stage: delay line + butterfly + output rotator.

Mirrors the DIF branch of SDFStageRadix2 in milovanovic/sdf-fft
SDFChainRadix2.scala, with the v0.1 simplifications of
doc/fixed-point-spec.md: zero pipeline registers, useGrow (word grows by one
bit per stage, no scaling and no saturation), register delay line, no
overflow detection. The surviving parts and their reference lines:

  inp         := io.in                                              (:376)
  load_input  := cntr < d                                           (:404)
  shift_in    := Mux(load_input, inp, butterfly_outputs(1))          (:409)
  shift_out   := ShiftRegister(shift_in, d, io.en)                   (:429)
  butt        := Butterfly(Seq(shift_out, inp)) = (sum, difference)  (:453)
  out         := Mux(load_output, feedback, butt_out_0)              (:504)
  io.out      := Mux(cntr < d && cntr != 0, rot(out, tw), out)       (:377, :391)

With zero add pipes, feedback == shift_out and load_output == load_input, so
the two muxes share one condition. Butterfly(Seq(a, b)) returns (a+b, a-b) and
is fed (shift_out, inp), so the sum is shift_out + inp and the difference is
shift_out - inp -- that order is verified in SDFFFTUtil.scala and is not
symmetric, getting it backwards negates every odd output bin.

Equivalent to SdfStage.step() in python/sdf_fixed.py, which is the golden
model the tests compare against.

The whole stage is combinational: a sample enters and leaves in the same
cycle, and the delay line holds the only state. That is what gives the chain
its N-1 latency and lets the C++ be compared against the Python model with no
timing bookkeeping. It is not a synthesisable arrangement at N = 8192 --
thirteen butterflies and thirteen complex multiplies in one clock period --
and pipelining it is deferred work that must not change any output value.

cntr is the stage counter. Only its low log2(2d) bits matter, so a raw master
count may be passed in and the stage truncates; that truncation is the
hardware's (SDFChainRadix2.scala:220) and not a modelling convenience.

en low freezes the delay line. The output stays combinationally driven,
because in hardware it is just wires -- it is the consumer's job not to
sample it.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SdfDelayLine.hpp"
#include "SdfTypes.hpp"

#include <cstddef>
#include <vector>

namespace smfft {

class SdfStage : public Component {
  DECLARE_COMPONENT(SdfStage);

 public:
  // delay is the stage's d (power of two >= 1); B_w the twiddle width.
  SdfStage(std::string name, std::size_t delay, std::size_t B_w, COMPONENT_CTOR);
  ~SdfStage();

  Clock(clk);

  Input(SdfComplex,  in);
  Input(u32,         cntr);
  Input(bit,         en);
  Output(SdfComplex, out);

  std::size_t delay() const { return delay_; }
  std::size_t twiddleWidth() const { return B_w_; }

  void update();
  void reset();

 private:
  std::size_t delay_ = 1;
  std::size_t B_w_   = 16;

  // Stage constants, d entries, index 0 unused (multiplier bypassed).
  std::vector<SdfComplex> tw_;

  SdfDelayLine* dl_ = nullptr;

  // Parent-side drivers for the delay line's inputs. A sub-component's input
  // is wired once with <<, so the update writes these rather than reaching
  // into dl_ directly.
  Output(SdfComplex, dl_in_);
  Output(bit,        dl_en_);
};

} // namespace smfft
