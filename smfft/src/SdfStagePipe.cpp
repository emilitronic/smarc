// **********************************************************************
// smfft/src/SdfStagePipe.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026

#include "SdfStagePipe.hpp"

#include "SdfMultiply.hpp"
#include "SdfTwiddle.hpp"

namespace smfft {

SdfStagePipe::SdfStagePipe(std::string /*name*/, std::size_t delay, std::size_t B_w,
                           const SdfPipeParams& pipe, IMPL_CTOR)
    : delay_(delay), B_w_(B_w), pipe_(pipe) {
  assert_always(delay_ >= 1, "stage delay must be at least 1");
  assert_always((delay_ & (delay_ - 1)) == 0, "stage delay must be a power of two");
  assert_always(B_w_ >= 10 && B_w_ <= 24, "twiddle width must be 10..24");

  tw_ = twiddleTable(delay_, B_w_);

  dl_ = new SdfDelayLine("dl", delay_);
  dl_->clk << clk;
  dl_->in  << dl_in_;
  dl_->en  << dl_en_;

  apipe_ = new SdfPipe<SdfStageAWord>("apipe", pipe_.numAddPipes);
  apipe_->clk << clk;
  apipe_->in  << a_in_;
  apipe_->clr << no_clr_;

  cpipe_ = new SdfPipe<SdfStageCWord>("cpipe", pipe_.complexMulLatency());
  cpipe_->clk << clk;
  cpipe_->in  << c_in_;
  cpipe_->clr << no_clr_;

  // Three updates rather than one so that each pipe can be a register or a
  // wire without changing the dependency graph: front never reads anything
  // downstream of itself, and the delay-line loop stays inside front.
  UPDATE(updateFront).reads(in, ctl, dl_->out).writes(dl_in_, dl_en_, a_in_, no_clr_);
  UPDATE(updateMid).reads(apipe_->out).writes(c_in_);
  UPDATE(updateBack).reads(cpipe_->out).writes(out);
}

SdfStagePipe::~SdfStagePipe() {
  delete dl_;
  delete apipe_;
  delete cpipe_;
}

void SdfStagePipe::updateFront() {
  const SdfCtl      k = *ctl;
  const std::size_t c = static_cast<std::size_t>(k.cnt) % (2 * delay_);

  const SdfComplex shift_out = *dl_->out;
  const SdfComplex inp       = *in;

  // Butterfly(Seq(shift_out, inp)): sum first, difference second.
  const SdfComplex sum{shift_out.re + inp.re, shift_out.im + inp.im};
  const SdfComplex dif{shift_out.re - inp.re, shift_out.im - inp.im};

  const bool load = c < delay_;

  // The loop: unchanged from SdfStage, and with no register in it.
  dl_in_  = load ? inp : dif;
  dl_en_  = bit(k.en != 0 ? 1 : 0);
  no_clr_ = 0;

  SdfStageAWord a;
  a.feedback = shift_out;
  a.sum      = sum;
  // The ROM is read with this cycle's count and the word rides the A pipe
  // with the data. Entries at c >= d are never selected.
  a.tw   = load ? tw_[c] : SdfComplex{};
  a.load = load ? 1 : 0;
  a.mul  = (load && c != 0) ? 1 : 0;
  a_in_  = a;
}

void SdfStagePipe::updateMid() {
  const SdfStageAWord a = *apipe_->out;

  SdfStageCWord w;
  w.plain = a.load != 0 ? a.feedback : a.sum;
  w.mul   = a.mul;
  // Only computed when it will be selected; tw is {0, 0} otherwise and the
  // product would be meaningless rather than wrong.
  w.rot = a.mul != 0 ? rotate(w.plain, a.tw, B_w_) : SdfComplex{};
  c_in_ = w;
}

void SdfStagePipe::updateBack() {
  const SdfStageCWord w = *cpipe_->out;
  out                   = w.mul != 0 ? w.rot : w.plain;
}

void SdfStagePipe::reset() {
  dl_in_.reset(SdfComplex{});
  dl_en_.reset(0);
  a_in_.reset(SdfStageAWord{});
  c_in_.reset(SdfStageCWord{});
  no_clr_.reset(0);
  out.reset(SdfComplex{});
}

} // namespace smfft
