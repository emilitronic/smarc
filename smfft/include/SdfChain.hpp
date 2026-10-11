// **********************************************************************
// smfft/include/SdfChain.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
The radix-2 DIF SDF chain: log2(N) stages in series with one shared counter.

Mirrors the stage instantiation and counter distribution of SDFChainRadix2 in
milovanovic/sdf-fft, with the v0.1 simplifications of
doc/fixed-point-spec.md. Equivalent to SdfStream in python/sdf_fixed.py.

Delays are N/2, N/4, ... 1 -- longest stage first. That ordering is what makes
it DIF, and it is the reversal of delayLog2s at SDFChainRadix2.scala:27. The
chain holds N-1 complex words in total.

This component has no flow control. It is driven by an external en, and the
caller is responsible for the frame protocol: feeding N samples, flushing with
N-1 more steps, and discarding the first N-1 outputs. The FSM
(sIdle/sProcess/sFlush), the input handshake and the output queue are a
separate layer still to be built, and none of them change an output value --
the spec is values and order only.

Counter distribution. Every stage is handed the same master count, and each
truncates it to its own log2(2d) bits. The reference instead gives stage idx
the count offset by the cumulative delay of the stages before it
(SDFChainRadix2.scala:220), which looks like a difference but is not: that
offset is N - N/2^idx, and the stage's own modulus is N/2^idx, so the offset
is congruent to zero and vanishes. It only does work when fewer than all
stages are active, which is the run-time-configurable size case v0.1 does not
cover. SdfStream omits it for the same reason.

That identity holds only because every stage sees the same count in the same
cycle, which in turn holds only because there are no pipeline registers
(SDFChainRadix2.scala:180-189 delays each stage's (en, cnt) by outputLatency,
here zero). Pipelining this later means the offset comes back.

Latency is N-1 with no pipeline registers: the first valid output appears when
the last stage's counter reaches N-2 (:191, :198).
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SdfStage.hpp"
#include "SdfTypes.hpp"

#include <cstddef>
#include <vector>

namespace smfft {

class SdfChain : public Component {
  DECLARE_COMPONENT(SdfChain);

 public:
  SdfChain(std::string name, const SdfParams& params, COMPONENT_CTOR);
  ~SdfChain();

  Clock(clk);

  Input(SdfComplex,  in);
  Input(bit,         en);
  Output(SdfComplex, out);

  const SdfParams& params() const { return params_; }
  std::size_t      stages() const { return stages_.size(); }
  std::size_t      latency() const { return params_.N - 1; }

  // Inspection only.
  const SdfStage& stage(std::size_t i) const { return *stages_[i]; }
  std::size_t     count() const { return static_cast<std::size_t>(*cnt_Q_); }

  void updateCount();
  void reset();

 private:
  SdfParams params_;
  std::vector<SdfStage*> stages_;

  // Master sample counter, modulo N. Advances only on an enabled cycle, so a
  // stalled chain holds both its counter and every stage's delay line.
  Output(u32, cnt_Q_);
  Register(u32, cnt_D_);
};

} // namespace smfft
