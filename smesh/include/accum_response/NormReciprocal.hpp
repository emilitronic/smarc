// **********************************************************************
// smesh/include/accum_response/NormReciprocal.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Computes 1/stddev, save the binary32 result in the selected stats slot, 
and signals the FSM whet it finishes.

TODO: This models a one-cycle result.  Add iterative arithmetic timing.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Computes 1/stddev and returns its binary32 bit pattern.
class NormReciprocal : public Component {
  DECLARE_COMPONENT(NormReciprocal);

 public:
  NormReciprocal(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(NormStateRegs, slot_states);
  Input(NormStatsRegs, stats);

  Output(bit,          started);
  Output(u8,           start_id);
  Output(bit,          finished);
  Output(u8,           finish_id);
  Output(u32,          result);

  void updateStart();
  void updateResult();
  void reset() override;

 private:
  Output(NormReciprocalPending,   pending_Q_);
  Register(NormReciprocalPending, pending_D_);
};

} // namespace smesh
