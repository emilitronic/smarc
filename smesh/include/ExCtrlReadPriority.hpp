// **********************************************************************
// smesh/include/ExCtrlReadPriority.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
/*
Combinational A/B/D read-priority gating for ExecuteController row feeds.
Prevents two operands from trying to read the same local-memory bank in the same cycle.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "ExCtrlOperandPack.hpp"

namespace smesh {

class ExCtrlReadPriority : public Component {
  DECLARE_COMPONENT(ExCtrlReadPriority);

 public:
  ExCtrlReadPriority(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(ExCtrlOperand, a_operand);
  Input(ExCtrlOperand, b_operand);
  Input(ExCtrlOperand, d_operand);
  Input(u32, total_rows);  // how many row-beats this mesh req should feed into systolic array
  Input(bit, im2col_wire); // external im2col req rdy
  Input(bit, im2col_en);

  Output(bit, a_valid); // A operand is not being held back by another operand
  Output(bit, b_valid);
  Output(bit, d_valid);

  void update();
};

} // namespace smesh
