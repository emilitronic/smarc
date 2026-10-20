// **********************************************************************
// smfft/include/SdfChainPipe.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
/*
The radix-2 DIF SDF chain built from SdfStagePipe: SdfChain with pipeline
registers.

Same stages, same delays (N/2 ... 1), same master counter. What pipelining
adds is the control distribution of SDFChainRadix2.scala:180-189: stage 0
gets (en, cnt) directly and each later stage gets the previous stage's
delayed by outputLatency = A + C registers, so the control for a sample
arrives at every stage in the same cycle as the sample does. The data
arrives L cycles late at each stage; so does its count; and each stage then
behaves exactly as it does unpipelined, just later.

That is also why the reference's cumulative-delay counter offset (:220)
still vanishes. SdfChain's header shows it is congruent to zero modulo every
stage's own counter width; what changes with pipelining is not that offset
but the time at which each stage sees its count, and the control pipes
already supply that. No other alignment term is needed.

The control pipes are free-running and are cleared by clr, as the
reference's ShiftRegisterWithReset is cleared in sIdle. The datapath pipes
are not cleared, as in the reference.

Latency is N - 1 + log2(N) * (A + C): N - 1 enabled cycles of fill, then
log2(N) * (A + C) clock cycles of pipeline (:247). With no stalls the input
accepted at cycle t produces output m = t - (N - 1) at t + log2(N) * (A + C).
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SdfPipe.hpp"
#include "SdfStagePipe.hpp"
#include "SdfTypes.hpp"

#include <cstddef>
#include <vector>

namespace smfft {

class SdfChainPipe : public Component {
  DECLARE_COMPONENT(SdfChainPipe);

 public:
  SdfChainPipe(std::string name, const SdfParams& params, const SdfPipeParams& pipe,
               COMPONENT_CTOR);
  ~SdfChainPipe();

  Clock(clk);

  Input(SdfComplex,  in);
  Input(bit,         en);
  // Clears the master counter and every control pipe; see SdfChain.
  Input(bit,         clr);
  Output(SdfComplex, out);
  Output(u32,        cntr);

  const SdfParams&     params() const { return params_; }
  const SdfPipeParams& pipe() const { return pipe_; }
  std::size_t          stages() const { return stages_.size(); }
  // Registers between the chain input and output.
  std::size_t pipeLatency() const { return stages_.size() * pipe_.stageLatency(); }
  std::size_t latency() const { return params_.N - 1 + pipeLatency(); }

  void updateCtl();
  void updateCount();
  void reset();

 private:
  SdfParams                  params_;
  SdfPipeParams              pipe_;
  std::vector<SdfStagePipe*> stages_;
  // ctlp_[i] feeds stage i + 1.
  std::vector<SdfPipe<SdfCtl>*> ctlp_;

  Output(SdfCtl, ctl0_); // stage 0's control, straight from en and the counter
  Register(u32,  cnt_D_);
};

} // namespace smfft
