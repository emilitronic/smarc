// **********************************************************************
// smesh/include/accum_response/AccScaleFinite.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
Finite-lane accumulator scaler: three row slots and four element scale pipes.
SlotCtrl accepts rows, selects the oldest completed output, and releases slots.
Regs stores incoming rows and assembles the element results into output rows.
Each Lane arbitrates its connected elements into arbOut; its Pipe scales them.

The external ports match AccScaleUnit. from_dma selects which consumer's ready
accepts the completed row. Every input row produces all kWidth output elements.
pipe_latency counts cycles after arbOut; slot capture, arbOut, and out_regs each
have their own clock boundary.

Current construction uses one group of four normalization-capable lanes, matching
Original's four normalization units when there are four total lanes. Every slot
uses this group, which supports ordinary scaling, ReLU, LayerNorm, and IGELU.
has_normalizations=false selects four ordinary lanes and rejects LayerNorm/IGELU.
has_nonlinear_activations=false bypasses activation and uses the ordinary scale.
WorkClass derives the lane-group policy from the saved rows' activations.
TODO: Add mixed lane groups when the total lane count exceeds the
normalization-capable lane count; WorkClass already supports that split.
*/
#pragma once

#include "AccScaleRegs.hpp"

#include <array>

namespace smesh {

class AccScaleSlotCtrl;
class AccScaleLane;
class AccScalePipe;
class AccScaleWorkClass;

class AccScaleFinite : public Component {
  DECLARE_COMPONENT(AccScaleFinite);

 public:
  AccScaleFinite(std::string name, int pipe_latency = 1, bool has_nonlinear_activations = true, bool has_normalizations = true, COMPONENT_CTOR);
  ~AccScaleFinite() override;
  static constexpr std::size_t kLanes = AccScaleRegs::kReturnLanes;

  Clock(clk);
  
  Input(bit,           req_val);
  Output(bit,          req_rdy);
  Input(AccScaleReq,   req_bits);

  Output(bit,          out_val);
  Output(AccScaleResp, out_bits);
  
  Input(bit,           out_rdy_issue);
  Input(bit,           out_rdy_exresp);

  void updateReady();
  void reset() override;

 private:
  AccScaleSlotCtrl* ctrl_ = nullptr;
  AccScaleRegs*     regs_ = nullptr;
  AccScaleWorkClass* work_class_ = nullptr;
  std::array<AccScaleLane*, kLanes> lanes_{};
  std::array<AccScalePipe*, kLanes> pipes_{};
  Output(bit, selected_out_rdy_);
};

} // namespace smesh
