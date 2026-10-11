// **********************************************************************
// smfft/src/SdfChain.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2026

#include "SdfChain.hpp"

namespace smfft {

SdfChain::SdfChain(std::string /*name*/, const SdfParams& params, IMPL_CTOR) : params_(params) {
  assert_always(params_.N >= 2, "N must be at least 2");
  assert_always((params_.N & (params_.N - 1)) == 0, "N must be a power of two");

  const std::size_t n = params_.stages();
  stages_.reserve(n);

  // Delays N/2, N/4, ... 1: longest first, which is what makes it DIF.
  for (std::size_t s = 1; s <= n; ++s) {
    const std::size_t d = params_.N >> s;
    stages_.push_back(new SdfStage("stage", d, params_.B_w));
  }

  for (std::size_t i = 0; i < n; ++i) {
    stages_[i]->clk  << clk;
    stages_[i]->en   << en;
    stages_[i]->cntr << cnt_Q_;
    // Each stage truncates the master count to its own log2(2d) bits; see the
    // header for why the reference's cumulative-delay offset is a no-op here.
    if (i == 0) {
      stages_[i]->in << in;
    } else {
      stages_[i]->in << stages_[i - 1]->out;
    }
  }
  out << stages_[n - 1]->out;

  cnt_Q_ <= cnt_D_;

  UPDATE(updateCount).reads(en, cnt_Q_).writes(cnt_D_);
}

SdfChain::~SdfChain() {
  for (auto* s : stages_) {
    delete s;
  }
}

void SdfChain::updateCount() {
  // Modulo N, so the counter is the sample index within the frame and every
  // stage's truncation of it is correct by construction.
  if (en == 1) {
    cnt_D_ = u32((static_cast<std::size_t>(*cnt_Q_) + 1) % params_.N);
  }
  // en == 0: cnt_D_ deliberately unassigned, so the counter holds.
}

void SdfChain::reset() { cnt_Q_.reset(0); cnt_D_.reset(0); }

} // namespace smfft
