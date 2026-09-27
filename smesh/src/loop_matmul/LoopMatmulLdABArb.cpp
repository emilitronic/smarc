// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulLdABArb.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulLdABArb.hpp"

namespace smesh {

LoopMatmulLdABArb::LoopMatmulLdABArb(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateOutput)
      .reads(a_val, a_bits, a_idle, a_k, a_i, a_loop_id, b_val, b_bits)
      .reads(b_idle, b_k, b_j, b_loop_id, head_loop_id, is_resadd)
      .writes(chosen_a_, out_val, out_bits);
  UPDATE(updateReady).reads(chosen_a_, out_rdy).writes(a_rdy, b_rdy);
}

void LoopMatmulLdABArb::updateOutput() {
  const bool same_loop = a_loop_id == b_loop_id;
  const bool force_a = is_resadd == 1
      ? same_loop && a_idle == 0
      : !same_loop && a_loop_id == head_loop_id;
  const bool force_b = is_resadd == 1
      ? (!same_loop && b_loop_id == head_loop_id) || a_idle == 1
      : !same_loop && b_loop_id == head_loop_id;

  // weightA=0 with staticWeightAEnabled: force, idle, then K position.
  const bool choose_a = force_a ? true :
      force_b ? false :
      a_idle == 1 ? false :
      b_idle == 1 ? true :
      !(a_k > b_k || (b_k == 0 && b_j == 0));

  out_val = choose_a ? *a_val : *b_val;
  out_bits = choose_a ? *a_bits : *b_bits;
  chosen_a_ = bit(choose_a);
}

void LoopMatmulLdABArb::updateReady() {
  a_rdy = bit(chosen_a_ == 1 && out_rdy == 1);
  b_rdy = bit(chosen_a_ == 0 && out_rdy == 1);
}

void LoopMatmulLdABArb::reset() {
  a_rdy.reset(0);
  b_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshCmd{});
  chosen_a_.reset(0);
}

} // namespace smesh
