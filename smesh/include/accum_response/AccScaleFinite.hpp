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

Current construction uses one group of four ordinary scale lanes (act == 0 or 1).
The has_nonlinear_activations parameter enables ReLU in each pipe; when disabled,
a ReLU request receives ordinary scaling without activation, as in Original.
TODO: Add work classification and normalization-capable lane groups when the
per-element activation/normalization operations are implemented.
*/
#pragma once

#include "AccScaleRegs.hpp"

#include <array>

namespace smesh {

class AccScaleSlotCtrl;
class AccScaleLane;
class AccScalePipe;

class AccScaleFinite : public Component {
  DECLARE_COMPONENT(AccScaleFinite);

 public:
  AccScaleFinite(std::string name, int pipe_latency = 1, bool has_nonlinear_activations = true, COMPONENT_CTOR);
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
  std::array<AccScaleLane*, kLanes> lanes_{};
  std::array<AccScalePipe*, kLanes> pipes_{};
  Output(bit, selected_out_rdy_);
};

} // namespace smesh
