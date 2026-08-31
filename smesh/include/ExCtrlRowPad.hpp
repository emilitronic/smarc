// **********************************************************************
// smesh/include/ExCtrlRowPad.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 15 2026
/*
Execute-controller row-padding validity helper.
Decides whether this current row/beat is real data or should be treated as zeros.
Input:
Decoder tells us how many rows and cols are in the current mesh request.
Row feed state tells us which row/beat is currently being fed into the spatial array.
Output:
Read request logic told that this row/beat is not all zeros.
Mesh cntl packet told how many unpadded columns are in this row/beat.
*/

#pragma once

#include <cascade/Cascade.hpp>

namespace smesh {

class ExCtrlRowPad : public Component {
  DECLARE_COMPONENT(ExCtrlRowPad);

 public:
  ExCtrlRowPad(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(u32, a_fire_counter); // which A row beat is being input into the spatial array
  Input(u32, b_fire_counter); 
  Input(u32, d_fire_counter); 
  Input(u16, a_rows); // how many A row-beats this mesh req should feed into systolic array
  Input(u16, b_rows); 
  Input(u16, d_rows); 
  Input(u16, a_cols); // how many A columns are present in this row-beat
  Input(u16, b_cols); 
  Input(u16, d_cols); 
  Input(u32, block_size);

  Output(bit, a_row_is_not_all_zeros); // current A row-beat is not all padding zeros
  Output(bit, b_row_is_not_all_zeros);
  Output(bit, d_row_is_not_all_zeros);
  Output(u32, a_unpadded_cols);  // how many real A elements are present in this row-beat
  Output(u32, b_unpadded_cols);
  Output(u32, d_unpadded_cols);

  void update();
};

} // namespace smesh
