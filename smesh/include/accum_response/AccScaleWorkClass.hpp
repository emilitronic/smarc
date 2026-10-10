// **********************************************************************
// smesh/include/accum_response/AccScaleWorkClass.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 9 2026
/*
Decide which saved rows use normalization-capable lanes versus ordinary lanes.
norm_mask bit s means valid slot s requests LayerNorm, IGELU, or softmax.
current_policy bit s means slot s uses the normalization-capable lane group.
These differ: ordinary rows can also use that group to share the work.

Original builds an eight-entry static policy table for three row slots. All
normalization rows keep their group. When their fraction is below the fraction
of normalization lanes, additional slots join that group, highest slot first,
until its rounded share of the three slots is reached. Invalid slots still
participate in this table; the lane arbiters separately check regs_val.

Lane counts are construction parameters, not runtime signals. With no
normalization lanes, policy is zero; rejecting unsupported activations belongs
to the functional path. Classifying softmax does not implement its arithmetic.
This block is purely combinational and owns no clocked state.
*/
#pragma once

#include "AccScaleRegs.hpp"

namespace smesh {

class AccScaleWorkClass : public Component {
  DECLARE_COMPONENT(AccScaleWorkClass);

 public:
  AccScaleWorkClass(std::string name, unsigned total_lanes = 4,
                   unsigned normalization_lanes = 4, COMPONENT_CTOR);
  static constexpr std::size_t kEntries = AccScaleRegs::kEntries;

  Clock(clk); // Cascade scheduling domain only; classification has no registers.
  InputArray(bit, regs_val, kEntries);
  InputArray(AccScaleReq, regs_bits, kEntries);
  Output(u3, norm_mask);
  Output(u3, current_policy);

  void updateClassification();
  void reset() override;

 private:
  std::array<u3, 1u << kEntries> static_assignment_policy_{};
};

} // namespace smesh
