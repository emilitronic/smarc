// **********************************************************************
// smesh/include/accum_response/Normalizer.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
 Two-slot accumulator normalizer with sum, max, mean, variance, SumExp,
 and inverse-standard-deviation paths.
 Reset returns the saved row and current statistics. Other supported commands
 update the selected slot without producing an output row.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "NormalizerState.hpp"
#include "NormTypes.hpp"

#include <array>

namespace smesh {

class NormRowChunk;
class NormSumLane;
class NormMaxLane;
class NormMeanDivide;
class NormSqrt;
class NormReciprocal;
class NormScale;
class NormStats;

class Normalizer : public Component {
  DECLARE_COMPONENT(Normalizer);

 public:
  Normalizer(std::string name, std::size_t reduce_lanes = kDim, COMPONENT_CTOR);
  ~Normalizer() override;

  Clock(clk);

  Input(bit,            req_val);
  Output(bit,           req_rdy);
  Input(AccNormReq,     req_bits);

  Output(bit,           resp_val);
  Input(bit,            resp_rdy);
  Output(AccNormReq,    resp_bits);
  Output(NormStatsRegs, stats_view);

  void updateView();
  void updateAdmission();
  void updateSlotCmds();
  void updateEvents();
  void updateSlots();
  void reset() override;

 private:
  NormState*      state_       = nullptr;
  NormRowChunk*   chunker_     = nullptr;
  NormRowChunk*   max_chunker_ = nullptr;
  NormSumLane*    sum_lane_    = nullptr;
  NormMaxLane*    max_lane_    = nullptr;
  NormMeanDivide* mean_divide_ = nullptr;
  NormSqrt*       sqrt_        = nullptr;
  NormReciprocal* reciprocal_  = nullptr;
  NormScale*      scale_       = nullptr;
  NormStats*      stats_       = nullptr;

  Output(bit,              allowed_req_val_);
  Input(bit,               state_req_rdy_);
  Input(bit,               state_accept_val_);
  Input(u8,                state_accept_id_);
  Input(bit,               state_out_val_);
  Input(u8,                state_out_id_);
  Input(NormStateRegs,     slot_states_);
  Input(bit,               chunk_val_);
  Input(NormChunk,         chunk_bits_);
  Input(bit,               max_chunk_val_);
  Input(NormChunk,         max_chunk_bits_);
  Input(bit,               divide_started_);
  Input(u8,                divide_start_id_);
  Input(bit,               divide_finished_);
  Input(u8,                divide_finish_id_);
  Input(bit,               sqrt_started_);
  Input(u8,                sqrt_start_id_);
  Input(bit,               sqrt_finished_);
  Input(u8,                sqrt_finish_id_);
  Input(bit,               reciprocal_started_);
  Input(u8,                reciprocal_start_id_);
  Input(bit,               reciprocal_finished_);
  Input(u8,                reciprocal_finish_id_);
  Input(bit,               scale_started_);
  Input(u8,                scale_start_id_);
  Input(bit,               scale_finished_);
  Input(u8,                scale_finish_id_);
  Output(NormSlotCmds,     slot_cmds_);
  Output(NormStateEvents,  events_);

  Output(NormSavedPackets,   saved_Q_); // holds current packet for each slot (row data & cmd)
  Register(NormSavedPackets, saved_D_);
};

} // namespace smesh
