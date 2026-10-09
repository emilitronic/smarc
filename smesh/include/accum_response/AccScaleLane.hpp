// **********************************************************************
// smesh/include/accum_response/AccScaleLane.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 8 2026
/*
One lane's connections from the three regs slots, round-robin arbiter, and
arbOut register. AccScalePipe follows this register and applies scaling.
Original connects a candidate when 
(slot * row_width + element) % group_lanes == lane_index.
current_policy selects which slots use normalization lanes. The policy itself
is supplied externally until we build work classification.
Original always accepts the arbiter output; arbOut is a valid-only register
that presents the selected element one cycle later to the scale pipe.
This lane owns fired_masks for its connected elements. Other bits stay zero.
*/
#pragma once

#include "AccScaleRegs.hpp"

namespace smesh {

// One element, its scale/normalization parameters, and where its result belongs.
struct AccScaleElem {
  Acc data          = 0;
  Acc full_data     = 0;
  u32 scale         = 0;
  u8 act            = 0;
  u32 igelu_qb      = 0;
  u32 igelu_qc      = 0;
  u32 iexp_qln2     = 0;
  u32 iexp_qln2_inv = 0;
  Acc mean          = 0;
  Acc max           = 0;
  u32 inv_stddev    = 0;
  u32 inv_sum_exp   = 0;
  u8 slot           = 0;
  u16 element       = 0;
};

class AccScaleLane : public Component {
  DECLARE_COMPONENT(AccScaleLane);
 public:
  // lane_index is within the normalization or ordinary group, not a global lane ID.
  AccScaleLane(std::string name, unsigned lane_index = 0, unsigned group_lanes = 4, bool normalization_lane = true, COMPONENT_CTOR);
  static constexpr std::size_t kEntries = AccScaleRegs::kEntries;
  static constexpr std::size_t kWidth   = AccScaleRegs::kWidth;
  using FiredMask                       = std::array<bit, kWidth>;

  Clock(clk);
  InputArray(bit,         regs_val, kEntries);
  InputArray(AccScaleReq, regs_bits, kEntries);
  Input(u3,               current_policy); // bit s: slot s uses normalization-capable lanes
  Input(bit,              req_fire); // new packet enters the regs slot selected by tail_oh
  Input(u3,               tail_oh);

  Output(bit,             arb_val); // selected element is accepted into arbOut this cycle
  Output(AccScaleElem,    arb_bits);
  Output(bit,             arb_out_val_Q_); // Original arbOut.valid, visible next cycle
  Output(AccScaleElem,    arb_out_bits_Q_);
  OutputArray(FiredMask,  fired_masks_Q_, kEntries);

  void updateArbiter();
  void updateRegisters();
  void reset() override;

 private:
  const unsigned lane_index_;
  const unsigned group_lanes_;
  const bool normalization_lane_;
  Output(u16,              last_grant_Q_); // flattened index last accepted by the arbiter
  Register(u16,            last_grant_D_);
  Register(bit,            arb_out_val_D_);
  Register(AccScaleElem,   arb_out_bits_D_);
  RegisterArray(FiredMask, fired_masks_D_, kEntries);
};

} // namespace smesh
