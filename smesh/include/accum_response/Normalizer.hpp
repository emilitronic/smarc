// **********************************************************************
// smesh/include/accum_response/Normalizer.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 12 2026
/*
 Two-slot accumulator normalization shell. Arithmetic lanes are added separately.
 Uses NormState to accept packets into two registered stats slots and return 
 Reset packets.  A slot can accept a new packet in the same cycle its previous
 output is consumed.  Commands that need arithmetic lanes are backpressured
 until those lanes are implemented.
*/
#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"
#include "NormalizerState.hpp"

#include <array>

namespace smesh {

struct NormSavedPackets {
  std::array<AccNormReq, kNormStatsSlots> packet{};
};

class Normalizer : public Component {
  DECLARE_COMPONENT(Normalizer);

 public:
  Normalizer(std::string name, COMPONENT_CTOR);
  ~Normalizer() override;

  Clock(clk);

  Input(bit,         req_val);
  Output(bit,        req_rdy);
  Input(AccNormReq,  req_bits);

  Output(bit,        resp_val);
  Input(bit,         resp_rdy);
  Output(AccNormReq, resp_bits);

  void updateView();
  void updateAdmission();
  void updateSlotCmds();
  void updateEvents();
  void updateSlots();
  void reset() override;

 private:
  NormState* state_ = nullptr;

  Output(bit,             allowed_req_val_);
  Input(bit,              state_req_rdy_);
  Input(bit,              state_accept_val_);
  Input(u8,               state_accept_id_);
  Input(bit,              state_out_val_);
  Input(u8,               state_out_id_);
  Output(NormSlotCmds,    slot_cmds_);
  Output(NormStateEvents, events_);

  Output(NormSavedPackets,   saved_Q_);
  Register(NormSavedPackets, saved_D_);
};

} // namespace smesh
