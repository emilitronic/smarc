// **********************************************************************
// smesh/include/accum_response/NormScale.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Scales inv_stddev or inv_sum_exp using the saved command's scale factor.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Multiplies a slot's selected reciprocal by its saved binary32 scale; result takes one cycle.
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
