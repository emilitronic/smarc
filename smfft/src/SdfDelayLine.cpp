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

  idx_Q_    <= idx_D_;
  w_val_Q_  <= w_val_D_;
  w_addr_Q_ <= w_addr_D_;
  w_data_Q_ <= w_data_D_;

  // Deliberately disjoint: the read path touches no input port, so a stage may
  // feed this component's input from its output in the same cycle.
  UPDATE(updateRead).reads(idx_Q_, w_val_Q_, w_addr_Q_, w_data_Q_).writes(out);
  UPDATE(updateWrite).reads(in, en, idx_Q_).writes(idx_D_, w_val_D_, w_addr_D_, w_data_D_);
}

void SdfDelayLine::updateRead() {
  // Commit the write captured at the previous clock edge, then read. At
  // depth 1 both hit the same address, and this order is what makes the
  // output last cycle's input rather than this cycle's.
  if (w_val_Q_ == 1) {
    mem_[static_cast<std::size_t>(*w_addr_Q_)] = *w_data_Q_;
  }

  out = mem_[static_cast<std::size_t>(*idx_Q_)];
}

void SdfDelayLine::updateWrite() {
  const std::size_t idx = static_cast<std::size_t>(*idx_Q_);

  // Cleared rather than left to hold: an unassigned register keeps its value,
  // which on a stalled cycle would commit the previous write a second time.
  w_val_D_ = en;

  if (en == 1) {
    w_addr_D_ = u32(idx);
    w_data_D_ = *in;
    idx_D_    = u32((idx + 1) % depth_);

    // out is not read here: it belongs to updateRead, and reading it from an
    // update that does not declare it would be an undeclared dependency.
    trace("sdf_delay_line: d=%u capture idx=%u re=%d im=%d\n",
          static_cast<unsigned>(depth_),
          static_cast<unsigned>(idx),
          in->re,
          in->im);
  }
  // en == 0: idx_D_ deliberately unassigned, so the address register holds.
}

void SdfDelayLine::reset() {
  mem_.assign(depth_, SdfComplex{});
  idx_Q_.reset(0);
  idx_D_.reset(0);
  w_val_Q_.reset(0);
  w_val_D_.reset(0);
  w_addr_Q_.reset(0);
  w_addr_D_.reset(0);
  w_data_Q_.reset(SdfComplex{});
  w_data_D_.reset(SdfComplex{});
  out.reset(SdfComplex{});
}

} // namespace smfft
