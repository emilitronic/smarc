// **********************************************************************
// smesh/include/accum_response/AccScalePipe.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
One lane's functional scale pipeline, following its arbOut register.
Accepts one element whenever in_val is asserted; no ready or output stalls.
The scaled, clipped result reaches out_regs after the configured latency.
Full-width data and destination slot/element travel with the result.

Original uses a Valid-only Pipe here. This model computes ordinary scaling
at the pipeline entrance and delays the result through explicit registers;
it does not model the internal floating-point arithmetic stages.
Activation and normalization operations will be added separately.
*/
#pragma once

#include "AccScaleLane.hpp"

namespace smesh {

// One clocked stage holds a result and whether that result is meaningful.
struct AccScalePipeEntry {
  bit            valid = 0;
  AccScaleResult bits{};
};

class AccScalePipe : public Component {
  DECLARE_COMPONENT(AccScalePipe);

 public:
  // Latency starts at in_val, after the lane's separate arbOut register.
  AccScalePipe(std::string name, int latency = 1, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,             in_val);
  Input(AccScaleElem,    in_bits);
  
  Output(bit,            out_val);
  Output(AccScaleResult, out_bits);

  void updateStages();
  void updateOutput();
  void reset() override;

 private:
  OutputArray(AccScalePipeEntry,   stages_Q_);
  RegisterArray(AccScalePipeEntry, stages_D_);
};

} // namespace smesh
