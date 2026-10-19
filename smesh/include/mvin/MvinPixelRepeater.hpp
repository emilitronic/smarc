// **********************************************************************
// smesh/include/mvin/MvinPixelRepeater.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Original's load-path pixel repeater. With repetition enabled, save one packet
and emit shifted copies into preceding SPAD rows, ending at its original row.
Copies never cross the start of the selected SPAD half. Only the final copy
retains last, so the memory write path acknowledges the source bytes once.

passthrough=true keeps the original combinational path. Otherwise input is
registered, one copy can leave per cycle, and the final copy can leave while
a new packet is accepted. Repetition is supported only for SPAD destinations.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

class MvinPixelRepeater : public Component {
  DECLARE_COMPONENT(MvinPixelRepeater);

 public:
  MvinPixelRepeater(std::string name,
                    bool passthrough = !kDefaultConfig.has_first_layer_optimizations,
                    COMPONENT_CTOR);

  Clock(clk);

  Input(bit, in_val);
  Input(DmaReadResp, in_bits);
  Output(bit, in_rdy);
  Output(bit, out_val);
  Output(DmaReadResp, out_bits);
  Input(bit, out_rdy);

  void updateView();
  void updateReady();
  void updateStorage();
  void reset();

 private:
  const bool passthrough_;
  // Saved source packet and the number of copies left after the current copy.
  struct Entry {
    bit valid = 0;
    DmaReadResp bits{};
    u8 remaining = 0;
  };
  Output(Entry, entry_Q_);
  Register(Entry, entry_D_);
  Output(bit, underflow_);
};

} // namespace smesh
