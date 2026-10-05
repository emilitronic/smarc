// **********************************************************************
// smesh/include/accum_response/NormSqrt.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Computes an exact integer square root of the select slot's variance.
NormStats stores the result and substitutes 1 when it is zero.

TODO: This models a one-cycle result.  Add iterative square-root timing.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Computes the integer square root of one slot's variance.
class NormSqrt : public Component {
  DECLARE_COMPONENT(NormSqrt);

 public:
  NormSqrt(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(NormStateRegs, slot_states);
  Input(NormStatsRegs, stats);

  Output(bit,          started);
  Output(u8,           start_id);
  Output(bit,          finished);
  Output(u8,           finish_id);
  Output(Acc,          result);

  void updateStart();
  void updateResult();
  void reset() override;

 private:
  Output(NormSqrtPending,   pending_Q_);
  Register(NormSqrtPending, pending_D_);
};

} // namespace smesh
