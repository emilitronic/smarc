// **********************************************************************
// smesh/src/accum_response/AccScalePipe.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026

#include "AccScalePipe.hpp"
#include "AccScaleMath.hpp"

TraceKey(acc_scale_pipe_);

namespace smesh {

AccScalePipe::AccScalePipe(std::string /*name*/, int latency, bool has_nonlinear_activations, IMPL_CTOR)
    : has_nonlinear_activations_(has_nonlinear_activations),
      stages_Q_(latency > 0 ? latency : 0),
      stages_D_(latency > 0 ? latency : 0) {
  assert_always(latency >= 1, "AccScalePipe latency must be at least one cycle");
  for (int stage = 0; stage < stages_Q_.size(); ++stage) {
    stages_Q_[stage] <= stages_D_[stage];
  }
  UPDATE(updateStages).reads(in_val, in_bits, stages_Q_).writes(stages_D_);
  UPDATE(updateOutput).reads(stages_Q_).writes(out_val, out_bits);
}

// Scale the arriving element; every clock moves earlier results one stage forward.
void AccScalePipe::updateStages() {
  AccScalePipeEntry incoming{};
  incoming.valid = *in_val;
  if (in_val == 1) {
    auto element = *in_bits;
    assert_always(element.act == 0 || element.act == 1, "AccScalePipe supports only NONE and RELU");
    // Original act=1: clamp negative input to zero before scaling; full_data stays unchanged.
    if (has_nonlinear_activations_ && element.act == 1 && element.data < 0) { // ReLU
      element.data = 0;                                                       // ReLU
    }                                                                         // ReLU
    incoming.bits      = scaleAccumulatorElement(element);
    trace(acc_scale_pipe_, "accept slot=%u element=%u full=%d scaled=%d\n",
          static_cast<unsigned>(incoming.bits.slot),
          static_cast<unsigned>(incoming.bits.element),
          static_cast<int>(incoming.bits.full_data),
          static_cast<int>(incoming.bits.data));
  }
  stages_D_[0] = incoming;
  // This bounded loop describes parallel clocked stage transfers, not extra work cycles.
  for (int stage = 1; stage < stages_Q_.size(); ++stage) {
    stages_D_[stage] = *stages_Q_[stage - 1];
  }
}

// Send the final stage's element result to its addressed location in out_regs.
void AccScalePipe::updateOutput() {
  const auto result = *stages_Q_[stages_Q_.size() - 1];
  out_val           = result.valid;
  out_bits          = result.valid == 1 ? result.bits : AccScaleResult{};
}

void AccScalePipe::reset() {
  for (int stage = 0; stage < stages_Q_.size(); ++stage) {
    stages_Q_[stage].reset(AccScalePipeEntry{});
    stages_D_[stage].reset(AccScalePipeEntry{});
  }
  out_val.reset(0);
  out_bits.reset(AccScaleResult{});
}

} // namespace smesh
