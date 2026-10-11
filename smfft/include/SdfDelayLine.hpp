// **********************************************************************
// smfft/include/SdfDelayLine.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 10 2026
/*
The d-word feedback delay line of one SDF stage.

Mirrors ShiftRegister(shift_in, delay, io.en) in milovanovic/sdf-fft
SDFChainRadix2.scala:429 and its ShiftRegisterMem/SRAM alternative at
:412-450. Modelled as a memory plus an address register rather than a
d-stage shift, because that is what the SRAM form actually is (address
counter plus memory) and because d reaches 4096 for the first stage at
N = 8192, where shifting every word each cycle would be pure waste.
Behaviour is identical either way: a value presented when enabled appears
on the output exactly d enabled cycles later.

State classification, per the cascade-component-design guidance:

  idx_Q_ / idx_D_   registered hardware state, explicit clock boundary
  mem_              behavioural memory: asynchronous read, synchronous
                    write, its clock boundary supplied by the address
                    register above. Same treatment smesh gives Spad::banks_.

Within update() the read is published before the write commits, which is
the read-during-write behaviour of the modelled memory and is what makes
the delay d rather than 0 when both hit the same address. That ordering is
intentional and local; it is not a dependence on update scheduling.

en low freezes the line: nothing is captured and idx_D_ is left unassigned,
so the address register holds and a stalled chain keeps its state.
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

  Input(SdfComplex,  in);
  Input(bit,         en);
  Output(SdfComplex, out);

  std::size_t depth() const { return depth_; }

  void update();
  void reset();

 private:
  std::size_t depth_ = 1;
  std::vector<SdfComplex> mem_;

  // Read/write address. Only the low ceil(log2(depth)) bits are meaningful;
  // 12 bits are needed for the largest stage at N = 8192.
  Output(u32, idx_Q_);
  Register(u32, idx_D_);
};

} // namespace smfft
