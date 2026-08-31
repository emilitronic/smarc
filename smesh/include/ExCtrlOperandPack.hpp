// **********************************************************************
// smesh/include/ExCtrlOperandPack.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
/*
Combinational operand packing for ExecuteController row-feed arbitration.
Groups per-operand signals into A/B/D operand records so shared arbitration 
code can process them uniformly.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshLocalAddr.hpp"

#include <cstdint>

namespace smesh {

struct ExCtrlOperand {
  SmeshLocalAddr addr{};
  bit            is_garbage       = 0;
  bit            start_inputting  = 0;
  std::uint32_t  counter          = 0; // = a/b/d_fire_counter = row progress counter
  bit            started          = 0; // = a/b/d_fire_started = whether A/B/D stream has begun
  bit            can_be_im2colled = 0;
  std::uint8_t   priority         = 0;
};

class ExCtrlOperandPack : public Component {
  DECLARE_COMPONENT(ExCtrlOperandPack);

 public:
  ExCtrlOperandPack(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(SmeshLocalAddr, a_address);
  Input(SmeshLocalAddr, b_address);
  Input(SmeshLocalAddr, d_address);
  Input(SmeshLocalAddr, a_address_rs1);
  Input(SmeshLocalAddr, b_address_rs2);
  Input(SmeshLocalAddr, d_address_rs1);
  Input(bit, start_inputting_a);
  Input(bit, start_inputting_b);
  Input(bit, start_inputting_d);
  Input(u32, a_fire_counter);
  Input(u32, b_fire_counter);
  Input(u32, d_fire_counter);
  Input(bit, a_fire_started);
  Input(bit, b_fire_started);
  Input(bit, d_fire_started);

  Output(ExCtrlOperand, a_operand);
  Output(ExCtrlOperand, b_operand);
  Output(ExCtrlOperand, d_operand);

  void update();
};

} // namespace smesh
