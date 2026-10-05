// **********************************************************************
// smesh/include/accum_response/NormReciprocal.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Computes 1/stddev or 127/sum_exp and signals the FSM when it finishes.

TODO: This models a one-cycle result.  Add iterative arithmetic timing.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Returns the selected reciprocal as a binary32 bit pattern.
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
