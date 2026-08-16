// **********************************************************************
// smesh/include/ExCtrlRowPad.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 15 2026
/*
Execute-controller row-padding validity helper.
*/

#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class ExCtrlRowPad : public Component {
  DECLARE_COMPONENT(ExCtrlRowPad);

 public:
  ExCtrlRowPad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(u32, a_fire_counter);
  Input(u32, b_fire_counter);
  Input(u32, d_fire_counter);
  Input(u16, a_rows);
  Input(u16, b_rows);
  Input(u16, d_rows);
  Input(u32, block_size);

  Output(bit, a_row_is_not_all_zeros);
  Output(bit, b_row_is_not_all_zeros);
  Output(bit, d_row_is_not_all_zeros);

  void update();
};

} // namespace smesh
