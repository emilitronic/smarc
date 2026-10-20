// **********************************************************************
// smfft/src/SdfStream.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "SdfStream.hpp"

namespace smfft {

SdfStream::SdfStream(std::string /*name*/, const SdfParams& params, IMPL_CTOR) : params_(params) {
  chain_ = new SdfChain("chain", params_);
  chain_->clk << clk;
  chain_->in  << chain_in_;
  chain_->en  << chain_en_;
  chain_->clr << chain_clr_;

  state_Q_  <= state_D_;
  filled_Q_ <= filled_D_;
  obin_Q_   <= obin_D_;

  // Split so that nothing reads what the chain produces from the same update
  // that drives the chain's inputs; that would be a combinational cycle
  // through the whole chain. chain_->cntr is safe to read here because it is
  // the Q side of a register, so there is no path from chain_en_ to it.
  UPDATE(updateCtrl)
      .reads(in_val, in_bits, in_last, out_rdy, state_Q_, filled_Q_, obin_Q_, chain_->cntr)
      .writes(in_rdy, out_val, out_last, busy)
      .writes(chain_in_, chain_en_, chain_clr_)
      .writes(state_D_, filled_D_, obin_D_);
  UPDATE(updateOut).reads(chain_->out).writes(out_bits);
}

SdfStream::~SdfStream() { delete chain_; }

void SdfStream::updateCtrl() {
  const unsigned    state  = static_cast<unsigned>(*state_Q_);
  const bool        filled = (filled_Q_ == 1);
  const std::size_t obin   = static_cast<std::size_t>(*obin_Q_);
  const std::size_t N      = params_.N;

  // Handshake. Nothing here reads a signal this update also writes: in_rdy is
  // a function of out_rdy and out_val a function of in_val, never the other
  // way round.
  bit rdy = 0;
  bit val = 0;
  switch (state) {
    case kIdle:
      rdy = 1; // accepting the first sample is what starts a frame
      val = 0;
      break;
    case kProcess:
      rdy = filled ? out_rdy : bit(1);
      val = filled ? in_val : bit(0);
      break;
    case kFlush:
      rdy = 0; // draining: no new samples until the frame is out
      val = 1;
      break;
    default:
      break;
  }
  in_rdy  = rdy;
  out_val = val;

  const bool in_fire  = (in_val == 1) && (rdy == 1);
  const bool out_fire = (val == 1) && (out_rdy == 1);

  // One enabled cycle per accepted sample, plus one per output drained during
  // the flush.
  const bool enable = in_fire || (state == kFlush && out_rdy == 1);

  // Zeros during the flush (see the header); otherwise in_bits whether or not
  // it is taken. At P = 0 out_bits is a function of chain_in_, so gating it
  // on in_fire would change a valid bin while out_rdy stalls it. Only
  // chain_en_ needs the fire.
  chain_in_ = state == kFlush ? SdfComplex{} : *in_bits;
  chain_en_ = bit(enable ? 1 : 0);

  // A sideband of the bin, held with it through a stall.
  out_last = bit((val == 1 && obin == N - 1) ? 1 : 0);
  busy     = bit(state == kIdle ? 0 : 1);

  // The last output of a frame ends the flush and returns the block to idle.
  const bool last_out = out_fire && obin == N - 1;

  unsigned next = state;
  switch (state) {
    case kIdle:
      if (in_fire) {
        next = kProcess;
      }
      break;
    case kProcess:
      if (in_fire && in_last == 1) {
        next = kFlush;
      }
      break;
    case kFlush:
      if (last_out) {
        next = kIdle;
      }
      break;
    default:
      break;
  }
  state_D_ = u32(next);

  // A flush burns N-1 enabled cycles beyond the frame, so the chain's counter
  // does not land back on zero by itself. Clear it as the frame ends.
  chain_clr_ = bit((next == kIdle && state != kIdle) ? 1 : 0);

  // Sticky from the cycle after the master counter reaches N-2, which is when
  // the last stage first has a real value to emit. Cleared with the frame.
  if (next == kIdle && state != kIdle) {
    filled_D_ = 0;
  } else if (enable && static_cast<std::size_t>(*chain_->cntr) == N - 2) {
    filled_D_ = 1;
  }

  if (last_out) {
    obin_D_ = u32(0);
  } else if (out_fire) {
    obin_D_ = u32(obin + 1);
  }

  trace("sdf_stream: state=%u filled=%u obin=%u cntr=%u in_val=%u in_rdy=%u out_val=%u "
        "out_rdy=%u en=%u\n",
        state,
        filled ? 1u : 0u,
        static_cast<unsigned>(obin),
        static_cast<unsigned>(*chain_->cntr),
        static_cast<unsigned>(*in_val),
        static_cast<unsigned>(rdy),
        static_cast<unsigned>(val),
        static_cast<unsigned>(*out_rdy),
        enable ? 1u : 0u);
}

void SdfStream::updateOut() { out_bits = *chain_->out; }

void SdfStream::reset() {
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SdfComplex{});
  out_last.reset(0);
  busy.reset(0);

  chain_in_.reset(SdfComplex{});
  chain_en_.reset(0);
  chain_clr_.reset(0);

  state_Q_.reset(kIdle);
  state_D_.reset(kIdle);
  filled_Q_.reset(0);
  filled_D_.reset(0);
  obin_Q_.reset(0);
  obin_D_.reset(0);
}

} // namespace smfft
