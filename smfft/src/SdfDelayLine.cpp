// **********************************************************************
// smfft/src/SdfDelayLine.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 10 2026

#include "SdfDelayLine.hpp"

namespace smfft {

SdfDelayLine::SdfDelayLine(std::string /*name*/, std::size_t depth, IMPL_CTOR)
    : depth_(depth), mem_(depth) {
  assert_always(depth_ >= 1, "delay line depth must be at least 1");
  assert_always((depth_ & (depth_ - 1)) == 0, "delay line depth must be a power of two");

  idx_Q_ <= idx_D_;

  UPDATE(update).reads(in, en, idx_Q_).writes(out, idx_D_);
}

void SdfDelayLine::update() {
  const std::size_t idx = static_cast<std::size_t>(*idx_Q_);

  // Current state out first: read-during-write at the same address returns
  // the stored word, which is what gives d cycles of delay.
  out = mem_[idx];

  if (en == 1) {
    mem_[idx] = *in;
    idx_D_ = u32((idx + 1) % depth_);
    trace("sdf_delay_line: d=%u idx=%u capture re=%d im=%d release re=%d im=%d\n",
          static_cast<unsigned>(depth_),
          static_cast<unsigned>(idx),
          in->re,
          in->im,
          out->re,
          out->im);
  }
  // en == 0: idx_D_ deliberately unassigned, so the register holds.
}

void SdfDelayLine::reset() {
  mem_.assign(depth_, SdfComplex{});
  idx_Q_.reset(0);
  idx_D_.reset(0);
  out.reset(SdfComplex{});
}

} // namespace smfft
