// **********************************************************************
// smesh/src/ExCtrlOperandPack.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlOperandPack.hpp"

namespace smesh {

namespace {

ExCtrlOperand packOperand(SmeshLocalAddr address,
                          SmeshLocalAddr base_address,
                          bit start_inputting,
                          std::uint32_t fire_counter,
                          bit fire_started,
                          bool can_be_im2colled,
                          std::uint8_t priority) {
  ExCtrlOperand operand{};
  operand.addr = address;
  operand.is_garbage = bit(base_address.is_garbage());
  operand.start_inputting = start_inputting;
  operand.counter = fire_counter;
  operand.started = fire_started;
  operand.can_be_im2colled = bit(can_be_im2colled);
  operand.priority = priority;
  return operand;
}

} // namespace

ExCtrlOperandPack::ExCtrlOperandPack(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_address,
             b_address,
             d_address,
             a_address_rs1,
             b_address_rs2,
             d_address_rs1,
             start_inputting_a,
             start_inputting_b)
      .reads(start_inputting_d,
             a_fire_counter,
             b_fire_counter,
             d_fire_counter,
             a_fire_started,
             b_fire_started,
             d_fire_started)
      .writes(a_operand, b_operand, d_operand);
}

void ExCtrlOperandPack::update() {
  a_operand = packOperand(*a_address,
                          *a_address_rs1,
                          *start_inputting_a,
                          static_cast<std::uint32_t>(*a_fire_counter),
                          *a_fire_started,
                          true,
                          0);
  b_operand = packOperand(*b_address,
                          *b_address_rs2,
                          *start_inputting_b,
                          static_cast<std::uint32_t>(*b_fire_counter),
                          *b_fire_started,
                          false,
                          1);
  d_operand = packOperand(*d_address,
                          *d_address_rs1,
                          *start_inputting_d,
                          static_cast<std::uint32_t>(*d_fire_counter),
                          *d_fire_started,
                          false,
                          2);
}

} // namespace smesh
