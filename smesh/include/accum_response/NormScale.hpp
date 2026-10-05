// **********************************************************************
// smesh/include/accum_response/NormScale.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Scales the inv_stddev statistic.  Multiply by command's scale factor and save the result in the selected slot.
scaled_inv_stddev = (1 / stddev) * scale
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Multiplies a slot's reciprocal by its saved binary32 scale; result takes one cycle.
class NormScale : public Component {
  DECLARE_COMPONENT(NormScale);

 public:
  NormScale(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(NormStateRegs,    slot_states);
  Input(NormStatsRegs,    stats);
  Input(NormSavedPackets, saved);

  Output(bit, started);
  Output(u8,  start_id);
  Output(bit, finished);
  Output(u8,  finish_id);
  Output(u32, result);

  void updateStart();
  void updateResult();
  void reset() override;

 private:
  Output(NormScalePending, pending_Q_);
  Register(NormScalePending, pending_D_);
};

} // namespace smesh
