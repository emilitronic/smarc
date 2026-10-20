// **********************************************************************
// smfft/include/SdfPipe.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 19 2026
/*
A free-running n-register pipeline: out(t) = in(t - n).

Mirrors ShiftRegister(x, n, true.B) and ShiftRegisterWithReset(x, n, init,
reset, true.B) in milovanovic/sdf-fft (SDFFFTUtil.scala:28-46). Every
pipeline register the reference puts in the SDF datapath is one of these, and
every one of them is enabled unconditionally: a stall does not freeze the
pipeline, it sends a bubble down it. Only the delay lines stop on en.

n = 0 is a plain wire, as it is in the reference (the recursion bottoms out
at `in`). That keeps the unpipelined configuration exactly the
combinational model SdfStage/SdfChain already are, with no special case in
the components that use this one.

For n >= 1 it has the same shape as SdfDelayLine, for the same reason: the
output is published from registered state only, so a parent may compute this
pipe's input from its output in the same cycle without Cascade seeing a
combinational cycle. A ring of n words plus a synchronous write buffer:
updateRead commits the write captured at the previous edge and then reads the
oldest word; updateWrite captures this cycle's input. Unlike SdfDelayLine
there is no enable and n need not be a power of two.

clr is the reference's manual reset: when high, every register is loaded with
T{} at the next edge and this cycle's input is dropped, reset taking priority
over the shift. Pipes the reference builds with plain ShiftRegister simply
leave it tied low.

T must be a plain struct with value-initialised members; T{} is the reset
value.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace smfft {

template <typename T>
class SdfPipe : public Component {
  DECLARE_COMPONENT(SdfPipe);

 public:
  SdfPipe(std::string /*name*/, std::size_t depth, COMPONENT_CTOR) : depth_(depth) {
    if (depth_ == 0) {
      UPDATE(updateWire).reads(in).writes(out);
      return;
    }
    mem_.assign(depth_, T{});

    idx_Q_    <= idx_D_;
    w_val_Q_  <= w_val_D_;
    w_data_Q_ <= w_data_D_;
    clr_Q_    <= clr_D_;

    UPDATE(updateRead).reads(idx_Q_, w_val_Q_, w_data_Q_, clr_Q_).writes(out);
    UPDATE(updateWrite).reads(in, clr, idx_Q_).writes(idx_D_, w_val_D_, w_data_D_, clr_D_);
  }

  Clock(clk);

  Input(T,   in);
  Input(bit, clr);
  Output(T,  out);

  std::size_t depth() const { return depth_; }

  void updateWire() { out = *in; }

  void updateRead() {
    const std::size_t idx = static_cast<std::size_t>(*idx_Q_);
    if (clr_Q_ == 1) {
      mem_.assign(depth_, T{});
    } else if (w_val_Q_ == 1) {
      // The slot written last edge is the one before idx; at depth 1 it is
      // idx itself, which is what makes out last cycle's input.
      mem_[(idx + depth_ - 1) % depth_] = *w_data_Q_;
    }
    out = mem_[idx];
  }

  void updateWrite() {
    const std::size_t idx = static_cast<std::size_t>(*idx_Q_);
    // Free-running: the ring advances every cycle. Cleared rather than held
    // so a reset cycle captures nothing.
    clr_D_    = clr;
    w_val_D_  = bit(clr == 1 ? 0 : 1);
    w_data_D_ = *in;
    idx_D_    = u32((idx + 1) % depth_);
  }

  void reset() {
    out.reset(T{});
    if (depth_ == 0) {
      return;
    }
    mem_.assign(depth_, T{});
    idx_Q_.reset(0);
    idx_D_.reset(0);
    w_val_Q_.reset(0);
    w_val_D_.reset(0);
    w_data_Q_.reset(T{});
    w_data_D_.reset(T{});
    clr_Q_.reset(0);
    clr_D_.reset(0);
  }

 private:
  std::size_t    depth_ = 0;
  std::vector<T> mem_;

  // Slot to read this cycle, which is also the slot the captured input will
  // be committed to: the oldest word is overwritten by the newest.
  Output(u32,   idx_Q_);
  Register(u32, idx_D_);
  Output(bit,   w_val_Q_);
  Register(bit, w_val_D_);
  Output(T,     w_data_Q_);
  Register(T,   w_data_D_);
  Output(bit,   clr_Q_);
  Register(bit, clr_D_);
};

} // namespace smfft
