// **********************************************************************
// smesh/include/accum_response/NormStats.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 4 2026
/*
Accumulates results separately for each stats slot.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "NormTypes.hpp"

namespace smesh {

// Holds each slot's running sum, maximum, count, and remaining row elements.
class NormStats : public Component {
  DECLARE_COMPONENT(NormStats);

 public:
  NormStats(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,           accept_val);
  Input(u8,            accept_id);
  Input(AccNormReq,    req_bits);
  Input(NormStateRegs, slot_states);
  Input(bit,           sum_chunk_val);
  Input(NormChunk,     sum_chunk_bits);
  Input(bit,           max_chunk_val);
  Input(NormChunk,     max_chunk_bits);
  Input(bit,           sum_val);
  Input(NormSumResult, sum_bits);
  Input(bit,           max_val);
  Input(NormMaxResult, max_bits);
  Input(bit,           divide_started);
  Input(u8,            divide_start_id);
  Input(bit,           divide_finished);
  Input(u8,            divide_finish_id);
  Input(Acc,           divide_result);
  Input(bit,           sqrt_finished);
  Input(u8,            sqrt_finish_id);
  Input(Acc,           sqrt_result);
  Input(bit,           reciprocal_finished);
  Input(u8,            reciprocal_finish_id);
  Input(u32,           reciprocal_result);

  Output(NormStatsRegs, view);

  void updateView();
  void updateState();
  void reset() override;

 private:
  Output(NormStatsRegs,   regs_Q_);
  Register(NormStatsRegs, regs_D_);
};

} // namespace smesh
