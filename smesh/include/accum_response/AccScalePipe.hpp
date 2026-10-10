// **********************************************************************
// smesh/include/accum_response/AccScalePipe.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
One lane's functional scale pipeline, following its arbOut register.
That is, the thing that does actual computation on elemen data.
Accepts one element whenever in_val is asserted; no ready or output stalls.
The scaled, clipped result reaches out_regs after the configured latency.
Full-width data and destination slot/element travel with the result.

Original uses a Valid-only Pipe here. This model computes activation and scaling
at the pipeline entrance and delays the result through explicit registers;
it does not model the internal floating-point arithmetic stages.
act == 1 selects ReLU before scaling when has_nonlinear_activations is true.
act == 2 selects layer normalization when both capability flags are true:
subtract mean at accumulator width, then use binary32 inv_stddev as the scale.
Normalizer has already included the ordinary row scale in inv_stddev.
act == 3 selects Original's integer IGELU polynomial using signed qb/qc bit
patterns when both capability flags are true; ordinary scaling follows it.
full_data keeps the original value. Other activation/normalization operations
will be added separately.

TODO: Implement act == 4 (softmax) only after defining a sound integer IEXP.
This operation is deliberately deferred, not an implemented softmax path.

What we found in Original:
- AccumulatorScale.scala's active iexp body duplicates its igelu polynomial;
  it does not use iexp_qln2/iexp_qln2_inv. Both the finite-lane and whole-row
  branches call this helper. Normalizer.scala uses the same arithmetic for
  SumExp/InvSumExp. Thus switching scaler branches does not resolve the issue.
- With q = element - max, the active helper gives iexp(0) == 0. The maximum
  element receives zero weight; an all-equal row gives a zero denominator.
  This is not the exponential weighting required by softmax.
- The commented-out exponent approximation is a candidate, not a validated
  fix. It decomposes q using qln2/qln2_inv, evaluates a polynomial, then shifts
  it. Original notes possible z overflow; its shift saturation checks bits
  5..15 only. Audit coefficient encodings, signedness, intermediate widths,
  subtraction/INT_MIN negation, overflow, and large shifts before adopting it.

Implementation contract to establish when resuming:
- Choose the approximation, coefficient format, supported input range, and
  quantization/error tolerance. Do not merely uncomment Original's code.
- Share one IEXP arithmetic helper between NormSumLane's SumExp/InvSumExp and
  this pipe: the denominator and each output numerator must use the same
  weights. Existing Normalizer tests check polynomial compatibility, not
  mathematical exponentiation or correct softmax; update them deliberately.
- The intended pipe path is accumulator-width (element - max), then IEXP,
  then binary32 inv_sum_exp scaling and clipping. Preserve original full_data,
  slot/element routing, capability gating, and configured pipeline latency.
- NormReciprocal currently computes 127 / signed(sum); NormScale then includes
  the ordinary row scale in inv_sum_exp. Zero sum yields infinity, which the
  scale arithmetic rejects. Define handling for zero/nonpositive sums and
  wrapped arithmetic rather than silently accepting an invalid denominator.

Verification needed:
- Positive weight at q == 0; nonnegative, monotonic weights over the supported
  range; all-equal rows approximately uniform; singleton approximately 127
  with identity row scale; a dominant element; signed/coefficient extremes
  and overflow/shift boundaries. Compare complete outputs with floating-point
  softmax using the agreed approximation and quantization tolerance.
- Exercise Normalizer -> scaler together, not just the IEXP helper; include
  DIM4/DIM8, bubbles, output backpressure, and unchanged full_data/metadata.
Relevant existing tests: tb_normalizer_sum_exp.cpp and
tb_normalizer_inv_sum_exp.cpp. The companion TODO is above iexp() in
smesh/src/accum_response/NormSumLane.cpp.
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
  // Latency (how many clocked stages work passes through before result comes out) 
  // starts at in_val, after the lane's separate arbOut register.
  AccScalePipe(std::string name, int latency = 1, bool has_nonlinear_activations = true,
               bool has_normalizations = true, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,             in_val);
  Input(AccScaleElem,    in_bits);

  Output(bit,            out_val);
  Output(AccScaleResult, out_bits);

  void updateStages();
  void updateOutput();
  void reset() override;

 private:
  const bool has_nonlinear_activations_;
  const bool has_normalizations_;
  OutputArray(AccScalePipeEntry,   stages_Q_);
  RegisterArray(AccScalePipeEntry, stages_D_);
};

} // namespace smesh
