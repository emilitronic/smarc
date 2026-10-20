// **********************************************************************
// smfft/src/SdfChainPipe.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026

#include "SdfChainPipe.hpp"

namespace smfft {

SdfChainPipe::SdfChainPipe(std::string /*name*/, const SdfParams& params,
                           const SdfPipeParams& pipe, IMPL_CTOR)
    : params_(params), pipe_(pipe) {
  assert_always(params_.N >= 2, "N must be at least 2");
  assert_always((params_.N & (params_.N - 1)) == 0, "N must be a power of two");

  const std::size_t n = params_.stages();
  const std::size_t L = pipe_.stageLatency();

  for (std::size_t s = 1; s <= n; ++s) {
    stages_.push_back(new SdfStagePipe("stage", params_.N >> s, params_.B_w, pipe_));
  }
  for (std::size_t i = 1; i < n; ++i) {
    ctlp_.push_back(new SdfPipe<SdfCtl>("ctlp", L));
  }

  for (std::size_t i = 0; i < n; ++i) {
    stages_[i]->clk << clk;
    if (i == 0) {
      stages_[i]->in  << in;
      stages_[i]->ctl << ctl0_;
    } else {
      auto* p = ctlp_[i - 1];
      p->clk << clk;
      p->clr << clr;
      if (i == 1) {
        p->in << ctl0_;
      } else {
        p->in << ctlp_[i - 2]->out;
      }
      stages_[i]->in  << stages_[i - 1]->out;
      stages_[i]->ctl << p->out;
    }
  }
  out << stages_[n - 1]->out;

  cntr <= cnt_D_;

  UPDATE(updateCtl).reads(en, cntr).writes(ctl0_);
  UPDATE(updateCount).reads(en, clr, cntr).writes(cnt_D_);
}

SdfChainPipe::~SdfChainPipe() {
  for (auto* s : stages_) {
    delete s;
  }
  for (auto* p : ctlp_) {
    delete p;
  }
}

void SdfChainPipe::updateCtl() {
  SdfCtl k;
  k.cnt = static_cast<std::uint32_t>(*cntr);
  k.en  = en == 1 ? 1 : 0;
  ctl0_ = k;
}

void SdfChainPipe::updateCount() {
  if (clr == 1) {
    cnt_D_ = u32(0);
  } else if (en == 1) {
    cnt_D_ = u32((static_cast<std::size_t>(*cntr) + 1) % params_.N);
  }
}

void SdfChainPipe::reset() {
  ctl0_.reset(SdfCtl{});
  cntr.reset(0);
  cnt_D_.reset(0);
}

} // namespace smfft
