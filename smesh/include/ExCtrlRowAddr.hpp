// **********************************************************************
// smesh/include/ExCtrlRowAddr.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
/*
Current row-address logic for ExecuteController operand feeding.
Computes local address for current row/beat of A, B, and D.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshLocalAddr.hpp"

namespace smesh {

class ExCtrlRowAddr : public Component {
  DECLARE_COMPONENT(ExCtrlRowAddr);

 public:
  ExCtrlRowAddr(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(SmeshLocalAddr, a_address_rs1); // A laddr from decoder-selected cmd slot
  Input(SmeshLocalAddr, b_address_rs2);
  Input(SmeshLocalAddr, d_address_rs1); // B laddr in rs1, for PRELOAD in WS
  Input(u32, a_addr_offset);
  Input(u32, b_fire_counter);
  Input(u32, d_fire_counter);
  Input(u32, block_size); // DIM
  Input(bit, ex_read_from_acc);
  Input(bit, ws_no_transpose);
  Input(u16, a_rows);
  Input(u16, b_rows);
  Input(bit, start_inputting_a);
  Input(bit, start_inputting_b);
  Input(bit, start_inputting_d);

  Output(SmeshLocalAddr, a_address);
  Output(SmeshLocalAddr, b_address);
  Output(SmeshLocalAddr, d_address);
  Output(u32, dataAbank);
  Output(u32, dataBbank);
  Output(u32, dataDbank);
  Output(u32, dataABankAcc);
  Output(u32, dataBBankAcc);
  Output(u32, dataDBankAcc);
  Output(bit, a_read_from_acc);
  Output(bit, b_read_from_acc);
  Output(bit, d_read_from_acc);
  Output(bit, a_garbage);
  Output(bit, b_garbage);
  Output(bit, d_garbage);
  Output(u32, total_rows);  // how many row-beats this mesh req should feed into systolic array

  void update();
};

} // namespace smesh
