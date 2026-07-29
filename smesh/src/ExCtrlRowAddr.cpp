// **********************************************************************
// smesh/src/ExCtrlRowAddr.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlRowAddr.hpp"

#include <cstdint>

namespace smesh {

namespace {

std::uint32_t reverseRowOffset(std::uint32_t block_size, std::uint32_t counter) {
  if (block_size == 0 || counter >= block_size) {
    return 0;
  }
  return block_size - 1 - counter;
}

} // namespace

ExCtrlRowAddr::ExCtrlRowAddr(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_address_rs1,
             b_address_rs2,
             d_address_rs1,
             a_addr_offset,
             b_fire_counter,
             d_fire_counter,
             block_size,
             ex_read_from_acc)
      .reads(start_inputting_a, start_inputting_b, start_inputting_d)
      .writes(a_address,
              b_address,
              d_address,
              dataAbank,
              dataBbank,
              dataDbank,
              dataABankAcc,
              dataBBankAcc)
      .writes(dataDBankAcc,
              a_read_from_acc,
              b_read_from_acc,
              d_read_from_acc,
              a_garbage,
              b_garbage,
              d_garbage);
}

void ExCtrlRowAddr::update() {
  const auto a_base = *a_address_rs1;
  const auto b_base = *b_address_rs2;
  const auto d_base = *d_address_rs1;

  const auto a_current = a_base + static_cast<std::uint32_t>(*a_addr_offset);
  const auto b_current = b_base + static_cast<std::uint32_t>(*b_fire_counter);
  const auto d_current = d_base + reverseRowOffset(static_cast<std::uint32_t>(*block_size), static_cast<std::uint32_t>(*d_fire_counter));

  a_address = a_current;
  b_address = b_current;
  d_address = d_current;

  dataAbank = a_current.sp_bank();
  dataBbank = b_current.sp_bank();
  dataDbank = d_current.sp_bank();

  dataABankAcc = a_current.acc_bank();
  dataBBankAcc = b_current.acc_bank();
  dataDBankAcc = d_current.acc_bank();

  a_read_from_acc = bit(ex_read_from_acc != 0 && a_base.is_acc_addr());
  b_read_from_acc = bit(ex_read_from_acc != 0 && b_base.is_acc_addr());
  d_read_from_acc = bit(ex_read_from_acc != 0 && d_base.is_acc_addr());

  a_garbage = bit(a_base.is_garbage() || start_inputting_a == 0);
  b_garbage = bit(b_base.is_garbage() || start_inputting_b == 0);
  d_garbage = bit(d_base.is_garbage() || start_inputting_d == 0);
}

} // namespace smesh
