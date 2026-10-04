// **********************************************************************
// smesh/include/accum_response/NormMeanDivide.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Accumulates Sum and Mean row chunks, divides the selected slot's signed sum by 
its count with truncation toward zero, and carries the stored mean on a later
Reset response.

TODO: Currently models a one-cycle delay, not realistic multi-cycle divide.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Divides a selected slot's signed sum by its count, with one cycle of latency.
class NormMeanDivide : public Component {
  DECLARE_COMPONENT(NormMeanDivide);

 public:
  NormMeanDivide(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(NormStateRegs, slot_states);
  Input(NormStatsRegs, stats);

  Output(bit, started);
  Output(u8,  start_id);
  Output(bit, finished);
  Output(u8,  finish_id);
  Output(Acc, result);

  void updateStart();
  void updateResult();
  void reset() override;

 private:
  Output(NormMeanPending,   pending_Q_);
  Register(NormMeanPending, pending_D_);
};

} // namespace smesh
