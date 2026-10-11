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

  UPDATE(update).reads(in, en).writes(out);
}

void SdfDelayLine::update() {
  // Present the oldest stored word first, then overwrite that same slot.
  // This read-before-write is what makes the delay d rather than d-1.
  out = mem_[idx_];
  if (en == 0) {
    return; // frozen: nothing captured, index held
  }
  mem_[idx_] = *in;
  idx_ = (idx_ + 1) % depth_;
}

void SdfDelayLine::reset() {
  mem_.assign(depth_, SdfComplex{});
  idx_ = 0;
  out.reset(SdfComplex{});
}

} // namespace smfft
