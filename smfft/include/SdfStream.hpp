// **********************************************************************
// smfft/include/SdfStream.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026
/*
Flow control around SdfChain: valid/ready handshake, frame framing, and the
three-state FSM. This is the first block in smfft that is usable on its own.

Mirrors the control half of SDFChainRadix2 in milovanovic/sdf-fft:

  sIdle / sProcess / sFlush                                     (:52, :73-92)
  enableInit = in.fire || (state === sFlush && out.ready)       (:164)
  cnt resets in idle, increments on enableInit                  (:166-171)
  first output when the last stage's counter reaches N-2        (:191, :198)
  busy := state =/= sIdle                                      (:293)

Note the reference's own doc says busy covers sFlush only; the code says
anything but idle, and the code is what is mirrored here.

With zero pipeline registers the output queue is bypassed entirely (:267), so
this is a pass-through: one accepted input yields one output in the same
cycle, once the N-1 fill is done. That makes the handshake simple and
cycle-free -- out_val follows in_val, in_rdy follows out_rdy -- and it is the
whole reason the v0.1 zero-latency choice was worth making.

  state     in_rdy          out_val
  sIdle     1               0
  sProcess  filled ? out_rdy : 1    filled ? in_val : 0
  sFlush    0               1

"filled" is the sticky flag set when the master counter reaches N-2. Before
it, the block swallows the N-1 samples that fill the chain and produces
nothing.

Flushing. in_last marks the last sample of a frame and moves the FSM to
sFlush, where the block stops accepting and pushes N-1 zeros through to drain
the chain. Frames stream back to back without any of this -- the counter wraps
to zero exactly on a frame boundary -- so in_last belongs at the end of a
dwell, not on every frame. Flushing each frame roughly halves throughput.

Zeros, not stale data. The reference leaves io.in.bits on the chain input
during sFlush, which writes junk into stage 0's delay line. That is harmless
rather than correct by design: the stage delays sum to N-1, so stage 0's
polluted output reaches the chain output only during cycles N/2-1 .. N-2, all
inside the discarded fill window of the next frame. Driving explicit zeros
costs nothing and removes the need to rely on that argument.

Timing is not part of doc/fixed-point-spec.md -- the contract is values and
order. The property this block must hold is therefore that the output sequence
is unchanged by any pattern of input gaps and downstream stalls, which is what
tb_sdf_stream checks against the canonical vectors.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SdfChain.hpp"
#include "SdfTypes.hpp"

#include <cstddef>

namespace smfft {

class SdfStream : public Component {
  DECLARE_COMPONENT(SdfStream);

 public:
  SdfStream(std::string name, const SdfParams& params, COMPONENT_CTOR);
  ~SdfStream();

  Clock(clk);

  // Sample input. in_last marks the final sample of a frame and requests a
  // flush; leave it low to stream frames back to back.
  Input(bit,         in_val);
  Input(SdfComplex,  in_bits);
  Input(bit,         in_last);
  Output(bit,        in_rdy);

  // Bin output, in bit-reversed order (spec section 6). out_last marks the
  // last bin of a frame.
  Output(bit,        out_val);
  Output(SdfComplex, out_bits);
  Output(bit,        out_last);
  Input(bit,         out_rdy);

  // High whenever a frame is in flight, i.e. the state is not idle.
  Output(bit,        busy);

  const SdfParams& params() const { return params_; }
  std::size_t      latency() const { return params_.N - 1; }

  void updateCtrl();
  void updateOut();
  void reset();

 private:
  enum State : unsigned { kIdle = 0, kProcess = 1, kFlush = 2 };

  SdfParams params_;
  SdfChain* chain_ = nullptr;

  // Parent-side drivers for the chain's inputs.
  Output(SdfComplex, chain_in_);
  Output(bit,        chain_en_);
  Output(bit,        chain_clr_);

  Output(u32,   state_Q_);
  Register(u32, state_D_);

  // Sticky: the chain has seen its first N-1 samples and its output is live.
  Output(bit,   filled_Q_);
  Register(bit, filled_D_);

  // Bin index within the frame, for out_last and for ending the flush.
  Output(u32,   obin_Q_);
  Register(u32, obin_D_);
};

} // namespace smfft
