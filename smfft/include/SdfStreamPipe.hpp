// **********************************************************************
// smfft/include/SdfStreamPipe.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
/*
Flow control around SdfChainPipe: SdfStream for a pipelined chain.

With P = log2(N) * (A + C) > 0 pipeline registers the block is no longer a
pass-through. A bin leaves the chain P cycles after the enable that produced
it, by which time downstream may have stopped accepting, and the pipeline
registers are free-running so they cannot be held. The reference's answer
(SDFChainRadix2.scala:247-292), mirrored here:

  outValid   enable && filled, delayed P cycles to travel with its bin
  outQueue   P + 1 entries, pipe = true, flow = true, never bypassed (:252)
  in.ready   forced high for P cycles past the fill, otherwise out.ready (:260)

The queue cannot overflow. Once filled, a sample is accepted only on a cycle
when out_rdy is high, and on such a cycle the queue either dequeues one or is
empty. So bins in the queue plus bins in flight never grows past P + 1, and
P + 1 entries hold the worst case: downstream stalls with P bins in flight.
assert_always checks it on every cycle; maxOccupancy() reports the high-water
mark so a test can show the bound being reached.

Where this departs from the reference, and why:

  - Flush length. The reference enables the chain on every sFlush cycle
    that downstream is ready (:164), with nothing bounding the count. With a
    pipeline the flush lasts P cycles longer than the N - 1 enables the frame
    needs, the surplus enables are marked valid, and the queue is not reset
    on the way back to idle. Rather than depend on that working out, the
    flush here issues exactly N - 1 enables and then waits for the queue to
    drain, and asserts the queue is empty when it returns to idle.
  - End of flush. The reference skips at most one earlier frame's last bin
    while flushing (lastWait, :119-138). With P >= N -- small N, deep
    pipeline -- several earlier frames can still be in flight when in_last
    arrives. Here the flush ends on the last bin of the last frame accepted,
    counted with frames_Q_.
  - in_rdy in sIdle is 1 and in sFlush is 0, as in SdfStream, rather than
    the reference's expression, which accepts input during a flush that
    started before the chain filled.
  - in_last must arrive on the last sample of a frame; that is asserted.

None of these change a value, and at P = 0 the block is SdfStream: the queue
and valid pipe are bypassed exactly as the reference bypasses them at
latency 0 (:267).

Timing is outside doc/fixed-point-spec.md. The property that matters is that
the bin sequence is the vectors' whatever the input gaps and downstream
stalls, which is what tb_sdf_stream_pipe checks.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SdfChainPipe.hpp"
#include "SdfPipe.hpp"
#include "SdfTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace smfft {

// Carried down the valid pipe: v is outValid, f is the fill flag that the
// forced-ready window is timed from.
struct SdfVF {
  std::uint8_t v = 0;
  std::uint8_t f = 0;
};

class SdfStreamPipe : public Component {
  DECLARE_COMPONENT(SdfStreamPipe);

 public:
  SdfStreamPipe(std::string name, const SdfParams& params, const SdfPipeParams& pipe,
                COMPONENT_CTOR);
  ~SdfStreamPipe();

  Clock(clk);

  Input(bit,         in_val);
  Input(SdfComplex,  in_bits);
  Input(bit,         in_last);
  Output(bit,        in_rdy);

  // As SdfStream: out_last is held with the last bin of a frame, and a valid
  // bin holds until out_rdy takes it, provided the source holds in_val and
  // in_bits until in_rdy. At P > 0 a stalled bin waits in the queue.
  Output(bit,        out_val);
  Output(SdfComplex, out_bits);
  Output(bit,        out_last);
  Input(bit,         out_rdy);

  Output(bit,        busy);

  const SdfParams&     params() const { return params_; }
  const SdfPipeParams& pipe() const { return pipe_; }
  std::size_t          pipeLatency() const { return P_; }
  std::size_t          latency() const { return params_.N - 1 + P_; }
  std::size_t          queueEntries() const { return P_ + 1; }
  std::size_t          maxOccupancy() const { return max_occ_; }

  void updateCtrl();
  void updateOut();
  void reset();

 private:
  enum State : unsigned { kIdle = 0, kProcess = 1, kFlush = 2 };

  SdfParams     params_;
  SdfPipeParams pipe_;
  std::size_t   P_ = 0;

  SdfChainPipe*   chain_ = nullptr;
  SdfPipe<SdfVF>* vpipe_ = nullptr;

  Output(SdfComplex, chain_in_);
  Output(bit,        chain_en_);
  Output(bit,        chain_clr_);
  Output(SdfVF,      vp_in_);

  Output(u32,   state_Q_);
  Register(u32, state_D_);
  Output(bit,   filled_Q_);
  Register(bit, filled_D_);
  Output(u32,   obin_Q_);
  Register(u32, obin_D_);
  // Flush enables issued so far; the flush stops at N - 1.
  Output(u32,   flush_Q_);
  Register(u32, flush_D_);
  // Frames accepted whose last bin has not yet left.
  Output(u32,   frames_Q_);
  Register(u32, frames_D_);

  // The output queue: P + 1 words, head and count registered, and the
  // write captured at one edge committed by updateOut at the start of the
  // next cycle, as SdfDelayLine does.
  std::vector<SdfComplex> qmem_;
  Output(u32,        q_head_Q_);
  Register(u32,      q_head_D_);
  Output(u32,        q_count_Q_);
  Register(u32,      q_count_D_);
  Output(bit,        q_w_val_Q_);
  Register(bit,      q_w_val_D_);
  Output(u32,        q_w_addr_Q_);
  Register(u32,      q_w_addr_D_);
  Output(SdfComplex, q_w_data_Q_);
  Register(SdfComplex, q_w_data_D_);

  std::size_t max_occ_ = 0; // inspection only
};

} // namespace smfft
