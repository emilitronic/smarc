// **********************************************************************
// smesh/include/accum_response/Normalizer.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
 Two-slot accumulator normalization shell with a sum reduction path.
 Reset packets return their saved row. Sum packets update the selected slot's
 running statistics without producing an output row.
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
class NormStats;

class Normalizer : public Component {
  DECLARE_COMPONENT(Normalizer);

 public:
  Normalizer(std::string name, std::size_t reduce_lanes = kDim, COMPONENT_CTOR);
  ~Normalizer() override;

  Clock(clk);

  Input(bit,         req_val);
  Output(bit,        req_rdy);
  Input(AccNormReq,  req_bits);

  Output(bit,        resp_val);
  Input(bit,         resp_rdy);
  Output(AccNormReq, resp_bits);
  Output(NormStatsRegs, stats_view);

  void updateView();
  void updateAdmission();
  void updateSlotCmds();
  void updateEvents();
  void updateSlots();
  void reset() override;

 private:
  NormState*    state_    = nullptr;
  NormRowChunk* chunker_  = nullptr;
  NormSumLane*  sum_lane_ = nullptr;
  NormStats*    stats_    = nullptr;

  Output(bit,              allowed_req_val_);
  Input(bit,               state_req_rdy_);
  Input(bit,               state_accept_val_);
  Input(u8,                state_accept_id_);
  Input(bit,               state_out_val_);
  Input(u8,                state_out_id_);
  Input(NormStateRegs,     slot_states_);
  Input(bit,               chunk_val_);
  Input(NormChunk,         chunk_bits_);
  Output(NormSlotCmds,     slot_cmds_);
  Output(NormStateEvents,  events_);

  Output(NormSavedPackets,   saved_Q_); // holds current packet for each slot (row data & cmd)
  Register(NormSavedPackets, saved_D_);
};

} // namespace smesh
