// **********************************************************************
// smfft/src/SdfStreamPipe.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026

#include "SdfStreamPipe.hpp"

namespace smfft {

SdfStreamPipe::SdfStreamPipe(std::string /*name*/, const SdfParams& params,
                             const SdfPipeParams& pipe, IMPL_CTOR)
    : params_(params), pipe_(pipe) {
  chain_ = new SdfChainPipe("chain", params_, pipe_);
  chain_->clk << clk;
  chain_->in  << chain_in_;
  chain_->en  << chain_en_;
  chain_->clr << chain_clr_;
  P_ = chain_->pipeLatency();

  vpipe_ = new SdfPipe<SdfVF>("vpipe", P_);
  vpipe_->clk << clk;
  vpipe_->in  << vp_in_;
  vpipe_->clr << chain_clr_;

  qmem_.assign(P_ + 1, SdfComplex{});

  state_Q_    <= state_D_;
  filled_Q_   <= filled_D_;
  obin_Q_     <= obin_D_;
  flush_Q_    <= flush_D_;
  frames_Q_   <= frames_D_;
  q_head_Q_   <= q_head_D_;
  q_count_Q_  <= q_count_D_;
  q_w_val_Q_  <= q_w_val_D_;
  q_w_addr_Q_ <= q_w_addr_D_;
  q_w_data_Q_ <= q_w_data_D_;

  if (P_ == 0) {
    // SdfStream's graph: out_val is a function of in_val, and out_bits comes
    // straight from the chain, which is combinational from chain_in_.
    UPDATE(updateCtrl)
        .reads(in_val, in_bits, in_last, out_rdy, chain_->cntr)
        .reads(state_Q_, filled_Q_, obin_Q_, flush_Q_, frames_Q_)
        .writes(in_rdy, out_val, out_last, busy)
        .writes(chain_in_, chain_en_, chain_clr_, vp_in_)
        .writes(state_D_, filled_D_, obin_D_, flush_D_, frames_D_);
    UPDATE(updateOut).reads(chain_->out).writes(out_bits);
  } else {
    // The output side depends on registers only -- the queue, the valid
    // pipe's tail and the chain's last pipeline register -- so out_val no
    // longer depends on anything upstream in the same cycle.
    UPDATE(updateOut)
        .reads(q_head_Q_, q_count_Q_, q_w_val_Q_, q_w_addr_Q_, q_w_data_Q_)
        .reads(vpipe_->out, chain_->out)
        .writes(out_val, out_bits);
    UPDATE(updateCtrl)
        .reads(in_val, in_bits, in_last, out_rdy, chain_->cntr)
        .reads(state_Q_, filled_Q_, obin_Q_, flush_Q_, frames_Q_)
        .reads(vpipe_->out, chain_->out, q_head_Q_, q_count_Q_)
        .writes(in_rdy, out_last, busy)
        .writes(chain_in_, chain_en_, chain_clr_, vp_in_)
        .writes(state_D_, filled_D_, obin_D_, flush_D_, frames_D_)
        .writes(q_head_D_, q_count_D_, q_w_val_D_, q_w_addr_D_, q_w_data_D_);
  }
}

SdfStreamPipe::~SdfStreamPipe() {
  delete chain_;
  delete vpipe_;
}

void SdfStreamPipe::updateOut() {
  if (P_ == 0) {
    out_bits = *chain_->out;
    return;
  }
  // Commit the enqueue captured at the previous edge, then present the head.
  if (q_w_val_Q_ == 1) {
    qmem_[static_cast<std::size_t>(*q_w_addr_Q_)] = *q_w_data_Q_;
  }
  const std::size_t count = static_cast<std::size_t>(*q_count_Q_);
  if (count > 0) {
    out_val  = 1;
    out_bits = qmem_[static_cast<std::size_t>(*q_head_Q_)];
  } else {
    // flow = true: an arriving bin is visible the cycle it arrives.
    out_val  = bit(vpipe_->out->v != 0 ? 1 : 0);
    out_bits = *chain_->out;
  }
}

void SdfStreamPipe::updateCtrl() {
  const unsigned    state  = static_cast<unsigned>(*state_Q_);
  const bool        filled = (filled_Q_ == 1);
  const std::size_t obin   = static_cast<std::size_t>(*obin_Q_);
  const std::size_t flushn = static_cast<std::size_t>(*flush_Q_);
  const std::size_t frames = static_cast<std::size_t>(*frames_Q_);
  const std::size_t cnt    = static_cast<std::size_t>(*chain_->cntr);
  const std::size_t N      = params_.N;

  // The fill flag as the input side should see it: P cycles late, so that
  // input stays open for the P cycles a pipelined chain takes to produce its
  // first bin. At P = 0 that is the flag itself.
  const bool filled_late = P_ == 0 ? filled : vpipe_->out->f != 0;

  bit rdy = 0;
  switch (state) {
    case kIdle:    rdy = 1; break;
    case kProcess: rdy = filled_late ? out_rdy : bit(1); break;
    default:       rdy = 0; break; // kFlush
  }
  in_rdy = rdy;

  // out_val, computed from the same state updateOut uses.
  bool val = false;
  if (P_ == 0) {
    val = state == kFlush || (state == kProcess && filled && in_val == 1);
    out_val = bit(val ? 1 : 0);
  } else {
    val = *q_count_Q_ > 0 || vpipe_->out->v != 0;
  }

  const bool in_fire  = (in_val == 1) && (rdy == 1);
  const bool out_fire = val && (out_rdy == 1);
  const bool flush_en = state == kFlush && out_rdy == 1 && flushn < N - 1;
  const bool enable   = in_fire || flush_en;

  assert_always(!(in_fire && in_last == 1) || cnt == N - 1,
                "in_last must mark the last sample of a frame");

  chain_in_ = in_fire ? *in_bits : SdfComplex{};
  chain_en_ = bit(enable ? 1 : 0);

  SdfVF vf;
  vf.v   = (enable && filled) ? 1 : 0;
  vf.f   = filled ? 1 : 0;
  vp_in_ = vf;

  const bool pkt_end = out_fire && obin == N - 1;
  out_last = bit(pkt_end ? 1 : 0);
  busy     = bit(state == kIdle ? 0 : 1);

  // The flush ends on the last bin of the last frame accepted, however many
  // earlier frames are still in the pipeline when in_last arrives.
  const bool last_out = pkt_end && state == kFlush && frames == 1;

  unsigned next = state;
  switch (state) {
    case kIdle:    if (in_fire) next = kProcess; break;
    case kProcess: if (in_fire && in_last == 1) next = kFlush; break;
    case kFlush:   if (last_out) next = kIdle; break;
    default:       break;
  }
  state_D_ = u32(next);

  const bool clr = next == kIdle && state != kIdle;
  chain_clr_     = bit(clr ? 1 : 0);

  const std::size_t frames_next =
      frames + ((in_fire && cnt == 0) ? 1 : 0) - (pkt_end ? 1 : 0);

  if (clr) {
    filled_D_ = 0;
    flush_D_  = u32(0);
    assert_always(frames_next == 0, "frames still open at end of flush");
    frames_D_ = u32(0);
  } else {
    if (enable && cnt == N - 2) {
      filled_D_ = 1;
    }
    if (flush_en) {
      flush_D_ = u32(flushn + 1);
    }
    frames_D_ = u32(frames_next);
  }

  if (pkt_end) {
    obin_D_ = u32(0);
  } else if (out_fire) {
    obin_D_ = u32(obin + 1);
  }

  if (P_ > 0) {
    const std::size_t cap   = P_ + 1;
    std::size_t       head  = static_cast<std::size_t>(*q_head_Q_);
    std::size_t       count = static_cast<std::size_t>(*q_count_Q_);
    const bool        enq   = vpipe_->out->v != 0;
    bool              write = false;
    std::size_t       waddr = 0;

    if (count == 0) {
      // Empty: a bin that is taken on arrival passes straight through.
      if (enq && !out_fire) {
        write = true;
        waddr = head;
        count = 1;
      }
    } else {
      if (enq) {
        write = true;
        waddr = (head + count) % cap;
        ++count;
      }
      if (out_fire) {
        head = (head + 1) % cap;
        --count;
      }
    }
    assert_always(count <= cap, "output queue overflow");
    if (clr) {
      assert_always(count == 0, "output queue not empty at end of flush");
    }
    if (count > max_occ_) {
      max_occ_ = count;
    }

    q_head_D_   = u32(head);
    q_count_D_  = u32(count);
    q_w_val_D_  = bit(write ? 1 : 0);
    q_w_addr_D_ = u32(waddr);
    q_w_data_D_ = *chain_->out;
  }

  trace("sdf_stream_pipe: state=%u filled=%u late=%u obin=%u cntr=%u flush=%u frames=%u "
        "in_val=%u in_rdy=%u out_val=%u out_rdy=%u en=%u\n",
        state,
        filled ? 1u : 0u,
        filled_late ? 1u : 0u,
        static_cast<unsigned>(obin),
        static_cast<unsigned>(cnt),
        static_cast<unsigned>(flushn),
        static_cast<unsigned>(frames),
        static_cast<unsigned>(*in_val),
        static_cast<unsigned>(rdy),
        val ? 1u : 0u,
        static_cast<unsigned>(*out_rdy),
        enable ? 1u : 0u);
}

void SdfStreamPipe::reset() {
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SdfComplex{});
  out_last.reset(0);
  busy.reset(0);

  chain_in_.reset(SdfComplex{});
  chain_en_.reset(0);
  chain_clr_.reset(0);
  vp_in_.reset(SdfVF{});

  state_Q_.reset(kIdle);
  state_D_.reset(kIdle);
  filled_Q_.reset(0);
  filled_D_.reset(0);
  obin_Q_.reset(0);
  obin_D_.reset(0);
  flush_Q_.reset(0);
  flush_D_.reset(0);
  frames_Q_.reset(0);
  frames_D_.reset(0);

  qmem_.assign(P_ + 1, SdfComplex{});
  q_head_Q_.reset(0);
  q_head_D_.reset(0);
  q_count_Q_.reset(0);
  q_count_D_.reset(0);
  q_w_val_Q_.reset(0);
  q_w_val_D_.reset(0);
  q_w_addr_Q_.reset(0);
  q_w_addr_D_.reset(0);
  q_w_data_Q_.reset(SdfComplex{});
  q_w_data_D_.reset(SdfComplex{});
  max_occ_ = 0;
}

} // namespace smfft
