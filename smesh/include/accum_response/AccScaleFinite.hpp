// **********************************************************************
// smesh/include/accum_response/AccScaleFinite.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
Finite-lane accumulator scaler: 
Three packet rows slots and configurable number of computational element pipes.
Packets are inputs from upstream normalizer consisting of a full row of kWidth
accumulator elements, a scale factor, and optional activation parameters.
SlotCtrl accepts rows, selects the oldest completed output, and releases slots.
Regs stores incoming rows and assembles the element results into output rows.
Each Lane arbitrates its connected elements into arbOut; its Pipe scales them.

The external ports match AccScaleUnit. from_dma selects which consumer's ready
accepts the completed row. Every input row produces all kWidth output elements.
pipe_latency counts cycles after arbOut; slot capture, arbOut, and out_regs each
have their own clock boundary.

With normalization enabled, the first four lanes support normalization, matching
Original. Additional lanes form an ordinary scaling/ReLU group. The default
four-lane construction uses just the normalization group; eight lanes give 4+4.
has_normalizations=false selects only ordinary lanes and rejects LayerNorm/IGELU.
has_nonlinear_activations=false bypasses activation and uses the ordinary scale.
WorkClass derives the lane-group policy from the saved rows' activations.
Shared fired masks remember every dispatched element across policy changes.
*/
#pragma once

#include "AccScaleRegs.hpp"

#include "AccScaleLane.hpp"
#include <vector>

namespace smesh {

class AccScaleSlotCtrl;
class AccScaleLane;
class AccScalePipe;
class AccScaleWorkClass;

class AccScaleFinite : public Component {
  DECLARE_COMPONENT(AccScaleFinite);

 // 
 public:
  AccScaleFinite(std::string name, int pipe_latency = 1, bool has_nonlinear_activations = true, bool has_normalizations = true, unsigned total_lanes = 4, COMPONENT_CTOR);
  ~AccScaleFinite() override;
  static constexpr std::size_t kDefaultLanes = AccScaleRegs::kReturnLanes;

  Clock(clk);
  
  Input(bit,           req_val);
  Output(bit,          req_rdy);
  Input(AccScaleReq,   req_bits);

  Output(bit,          out_val);
  Output(AccScaleResp, out_bits);
  
  Input(bit,           out_rdy_issue);
  Input(bit,           out_rdy_exresp);

  void updateReady();
  void updateDispatch();
  void reset() override;

 private:
  AccScaleSlotCtrl*                      ctrl_       = nullptr;
  AccScaleRegs*                          regs_       = nullptr;
  AccScaleWorkClass*                     work_class_ = nullptr;
  std::vector<AccScaleLane*>             lanes_;
  std::vector<AccScalePipe*>             pipes_;
  InputArray(bit,                        dispatch_val_);  // for observing what each lane accepted this ...
  InputArray(AccScaleElem,               dispatch_bits_); // ... cycle, to record which slot/element was dispatched
  OutputArray(AccScaleLane::FiredMask,   fired_masks_Q_, AccScaleRegs::kEntries);
  RegisterArray(AccScaleLane::FiredMask, fired_masks_D_, AccScaleRegs::kEntries);
  Input(bit,                             req_fire_);
  Input(u3,                              tail_oh_);
  Output(bit,                            selected_out_rdy_); // which consumer rdy sig controls o/p handshake (DMA or Ex)
};

} // namespace smesh
