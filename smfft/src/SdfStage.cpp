// **********************************************************************
// smfft/src/SdfStage.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "SdfStage.hpp"

#include "SdfMultiply.hpp"
#include "SdfTwiddle.hpp"

namespace smfft {

SdfStage::SdfStage(std::string /*name*/, std::size_t delay, std::size_t B_w, IMPL_CTOR)
    : delay_(delay), B_w_(B_w) {
  assert_always(delay_ >= 1, "stage delay must be at least 1");
  assert_always((delay_ & (delay_ - 1)) == 0, "stage delay must be a power of two");
  assert_always(B_w_ >= 10 && B_w_ <= 24, "twiddle width must be 10..24");

  tw_ = twiddleTable(delay_, B_w_);

  dl_ = new SdfDelayLine("dl", delay_);
  dl_->clk << clk;
  dl_->in  << dl_in_;
  dl_->en  << dl_en_;

  UPDATE(update).reads(in, cntr, en, dl_->out).writes(out, dl_in_, dl_en_);
}

SdfStage::~SdfStage() { delete dl_; }

void SdfStage::update() {
  // Truncation to log2(2d) bits, as the hardware does when it slices the
  // master count.
  const std::size_t c = static_cast<std::size_t>(*cntr) % (2 * delay_);

  const SdfComplex shift_out = *dl_->out;
  const SdfComplex inp       = *in;

  // Butterfly(Seq(shift_out, inp)): output 0 is the sum, output 1 the
  // difference, in that order.
  const SdfComplex sum{shift_out.re + inp.re, shift_out.im + inp.im};
  const SdfComplex dif{shift_out.re - inp.re, shift_out.im - inp.im};

  const bool load = c < delay_;

  // Mux 1: the delay line swallows the incoming sample during the fill half
  // of the block and the raw butterfly difference during the other half.
  dl_in_ = load ? inp : dif;
  dl_en_ = en;

  // Mux 2: the stage releases the stored word during fill, the butterfly sum
  // during butterfly.
  SdfComplex y = load ? shift_out : sum;

  // Mux 3: the rotator is bypassed at c == 0, where the twiddle is W^0 = 1.
  // tw_[0] is deliberately {0, 0}, so taking this branch wrongly zeroes the
  // output rather than quietly scaling it.
  if (load && c != 0) {
    y = rotate(y, tw_[c], B_w_);
  }

  out = y;

  trace("sdf_stage: d=%u c=%u en=%u in={%d,%d} dl_out={%d,%d} dl_in={%d,%d} out={%d,%d}\n",
        static_cast<unsigned>(delay_),
        static_cast<unsigned>(c),
        static_cast<unsigned>(*en),
        inp.re,
        inp.im,
        shift_out.re,
        shift_out.im,
        dl_in_->re,
        dl_in_->im,
        y.re,
        y.im);
}

void SdfStage::reset() {
  dl_in_.reset(SdfComplex{});
  dl_en_.reset(0);
  out.reset(SdfComplex{});
}

} // namespace smfft
