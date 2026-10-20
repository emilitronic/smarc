// **********************************************************************
// smfft/include/SdfStagePipe.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
/*
One radix-2 DIF SDF stage with the reference's pipeline registers.

SdfStage with numAddPipes (A) and numMulPipes (M) from milovanovic/sdf-fft
SDFStageRadix2, DIF branch (SDFChainRadix2.scala:334, :375-401, :498-504).
Every register is free-running (en = true.B in the reference), so a stall
sends a bubble through rather than freezing the stage; only the delay line
stops on en. At A = M = 0 every pipe is a wire and this is SdfStage, value for
value and cycle for cycle.

  front    shift_out, inp, butterfly, load = c < d, delay-line write     (comb)
  A regs   feedback  = SR(shift_out, A)                                   (:498)
           butt_out_0 = SR(sum, A)                                        (:499)
           load_output = SR(load, A)                                      (:501)
           tw = SR(rom(cntr), A)                                          (:334)
           multipleSignal = SR(c < d && c != 0, A)                        (:377)
  mid      out = load_output ? feedback : butt_out_0                      (:504)
  C regs   the multiplier, C = complexMulLatency                     (:347-351)
           ioOut = SR(out, C), select = SR(multipleSignal, C)        (:392-393)
  back     io.out = select ? out * tw : ioOut

where C = A + M with four real multipliers and 2A + M with three (the
default), because the three-multiplier form puts an adder in front of the
multiplies as well as after them. Stage latency is A + C.

The feedback loop -- delay-line read, butterfly difference, delay-line write
-- is untouched: the A registers sit after the butterfly on the output path
only, and the reference's `feedback` is the stored word on its way out, not
the loop. That is why neither knob changes a value.

The product is computed when the word enters the C pipe and then carried for
C cycles, rather than being split across them as the hardware does. The
spec's products are exact before the one rounding, so where the registers
fall inside the multiplier cannot change the result; only the latency is
modelled, and that is all there is to model.

ctl carries this stage's (cnt, en). The chain delays it by A + C per stage so
that it arrives with the data; see SdfChainPipe.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SdfDelayLine.hpp"
#include "SdfPipe.hpp"
#include "SdfTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace smfft {

// Pipeline depth knobs, named as in the reference's FFTParams.
struct SdfPipeParams {
  std::size_t numAddPipes = 0;
  std::size_t numMulPipes = 0;
  bool        use4Muls    = false; // reference default: the 3-multiplier form

  std::size_t complexMulLatency() const {
    return use4Muls ? numAddPipes + numMulPipes : 2 * numAddPipes + numMulPipes;
  }
  // outputLatency at SDFChainRadix2.scala:178.
  std::size_t stageLatency() const { return numAddPipes + complexMulLatency(); }
};

// A stage's control word: the counter value and the enable that go with the
// sample arriving this cycle.
struct SdfCtl {
  std::uint32_t cnt = 0;
  std::uint8_t  en  = 0;
};

// What crosses the A registers.
struct SdfStageAWord {
  SdfComplex   feedback;
  SdfComplex   sum;
  SdfComplex   tw;
  std::uint8_t load = 0;
  std::uint8_t mul  = 0;
};

// What crosses the C registers.
struct SdfStageCWord {
  SdfComplex   rot;
  SdfComplex   plain;
  std::uint8_t mul = 0;
};

class SdfStagePipe : public Component {
  DECLARE_COMPONENT(SdfStagePipe);

 public:
  SdfStagePipe(std::string name, std::size_t delay, std::size_t B_w, const SdfPipeParams& pipe,
               COMPONENT_CTOR);
  ~SdfStagePipe();

  Clock(clk);

  Input(SdfComplex,  in);
  Input(SdfCtl,      ctl);
  Output(SdfComplex, out);

  std::size_t delay() const { return delay_; }
  std::size_t latency() const { return pipe_.stageLatency(); }

  void updateFront();
  void updateMid();
  void updateBack();
  void reset();

 private:
  std::size_t             delay_ = 1;
  std::size_t             B_w_   = 16;
  SdfPipeParams           pipe_;
  std::vector<SdfComplex> tw_;

  SdfDelayLine*           dl_    = nullptr;
  SdfPipe<SdfStageAWord>* apipe_ = nullptr;
  SdfPipe<SdfStageCWord>* cpipe_ = nullptr;

  // Parent-side drivers for the children's inputs.
  Output(SdfComplex,    dl_in_);
  Output(bit,           dl_en_);
  Output(SdfStageAWord, a_in_);
  Output(SdfStageCWord, c_in_);
  Output(bit,           no_clr_); // the datapath pipes have no reset
};

} // namespace smfft
