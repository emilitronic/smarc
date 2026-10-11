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

The write is synchronous, and modelled as one: updateWrite captures the
address and data into registers, and updateRead commits that pending write
into mem_ at the start of the next cycle, before reading. A word presented
at cycle t therefore lands in the array at t+1 and is published at t+d,
which is the same d-cycle delay a read-before-write-in-one-update model
gives -- including at d = 1, where the commit and the read hit the same
address and out is last cycle's input.

Splitting it that way is what keeps out independent of in. An SDF stage
wires the butterfly difference back into this component's input while
reading its output in the same cycle, so a single update touching both
ports reports a combinational cycle and no update order exists -- even
though the hardware has none, because the read address comes from a
register. updateRead reads only registers; updateWrite writes only
registers.

en low freezes the line: w_val_D_ is cleared so nothing is captured, and
idx_D_ is left unassigned so the address register holds. w_val_D_ must be
assigned rather than left to hold, or a stalled cycle would replay the
previous write.
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

  void updateRead();
  void updateWrite();
  void reset();

 private:
  std::size_t depth_ = 1;
  std::vector<SdfComplex> mem_;

  // Read/write address. Only the low ceil(log2(depth)) bits are meaningful;
  // 12 bits are needed for the largest stage at N = 8192.
  Output(u32, idx_Q_);
  Register(u32, idx_D_);

  // The write captured at the previous clock edge, committed by updateRead.
  Output(bit, w_val_Q_);
  Register(bit, w_val_D_);
  Output(u32, w_addr_Q_);
  Register(u32, w_addr_D_);
  Output(SdfComplex, w_data_Q_);
  Register(SdfComplex, w_data_D_);
};

} // namespace smfft
