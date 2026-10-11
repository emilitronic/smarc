// **********************************************************************
// smfft/include/SdfDelayLine.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 10 2026
/*
The d-word feedback delay line of one SDF stage.

Mirrors ShiftRegister(shift_in, delay, io.en) in milovanovic/sdf-fft
SDFChainRadix2.scala:429, and its ShiftRegisterMem/SRAM alternative at
:412-450. Modelled as a circular buffer with one index rather than a
d-stage shift, because that is what the SRAM form actually is (address
counter plus memory) and because d reaches 4096 for the first stage at
N = 8192, where shifting every word each cycle would be pure waste.
Behaviour is identical either way: a value presented when enabled appears
on the output exactly d enabled cycles later.

en low freezes the line completely: nothing is captured and the index does
not advance, so a stalled chain holds its state.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SdfTypes.hpp"

#include <cstddef>
#include <vector>

namespace smfft {

class SdfDelayLine : public Component {
  DECLARE_COMPONENT(SdfDelayLine);

 public:
  // depth is the stage delay d, a power of two >= 1.
  SdfDelayLine(std::string name, std::size_t depth, COMPONENT_CTOR);

  Clock(clk);

  Input(SdfComplex, in);
  Input(bit, en);
  Output(SdfComplex, out);

  std::size_t depth() const { return depth_; }

  // Deliberately one update, not an output-view/storage pair. The read and
  // the write target the same slot, so their order is the whole behaviour:
  // read first gives d cycles of delay, write first gives d-1. Split across
  // two updates the only thing linking them is a plain C++ member, which the
  // scheduler cannot see, so the order would be its choice rather than ours
  // (and it picks write-first). Keeping both in one function makes the
  // ordering local and unambiguous.
  void update();
  void reset();

 private:
  std::size_t depth_ = 1;
  std::vector<SdfComplex> mem_;
  std::size_t idx_ = 0;
};

} // namespace smfft
