// **********************************************************************
// smesh/src/ExCtrlReadPriority.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlReadPriority.hpp"

namespace smesh {

ExCtrlReadPriority::ExCtrlReadPriority(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_operand, b_operand, d_operand, total_rows, im2col_wire, im2col_en)
      .writes(a_valid, b_valid, d_valid);
}

void ExCtrlReadPriority::update() {
  a_valid = 0;
  b_valid = 0;
  d_valid = 0;
}

} // namespace smesh
