// **********************************************************************
// smesh/src/ExCtrlRowAddr.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlRowAddr.hpp"

#include <algorithm>
#include <cstdint>

namespace smesh {

TraceKey(ex_ctrl_row_addr_view);

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
      .reads(ws_no_transpose,
             a_rows,
             b_rows)
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
              d_garbage,
              total_rows);
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

  const bool a_is_garbage = a_base.is_garbage() || start_inputting_a == 0;
  const bool b_is_garbage = b_base.is_garbage() || start_inputting_b == 0;
  const bool d_is_garbage = d_base.is_garbage() || start_inputting_d == 0;

  a_garbage = bit(a_is_garbage);
  b_garbage = bit(b_is_garbage);
  d_garbage = bit(d_is_garbage);

  const std::uint32_t rows_a = a_is_garbage ? 1u : static_cast<std::uint32_t>(*a_rows);
  const std::uint32_t rows_b = b_is_garbage ? 1u : static_cast<std::uint32_t>(*b_rows);
  const std::uint32_t next_total_rows = (ws_no_transpose != 0 && d_is_garbage)
      ? std::max(std::max(rows_a, rows_b), 4u)
      : static_cast<std::uint32_t>(*block_size);
  total_rows = next_total_rows;

  trace(ex_ctrl_row_addr_view,
        "start{a=%u b=%u d=%u} progress{aoff=%u b=%u d=%u} "
        "base{a=%05u b=%05u d=%05u} current{a=%05u b=%05u d=%05u} "
        "garbage{a=%u b=%u d=%u} bank{a=%u b=%u d=%u} total=%u\n",
        static_cast<unsigned>(start_inputting_a),
        static_cast<unsigned>(start_inputting_b),
        static_cast<unsigned>(start_inputting_d),
        static_cast<unsigned>(*a_addr_offset),
        static_cast<unsigned>(*b_fire_counter),
        static_cast<unsigned>(*d_fire_counter),
        static_cast<unsigned>(a_base.data()),
        static_cast<unsigned>(b_base.data()),
        static_cast<unsigned>(d_base.data()),
        static_cast<unsigned>(a_current.data()),
        static_cast<unsigned>(b_current.data()),
        static_cast<unsigned>(d_current.data()),
        static_cast<unsigned>(a_is_garbage),
        static_cast<unsigned>(b_is_garbage),
        static_cast<unsigned>(d_is_garbage),
        static_cast<unsigned>(a_current.sp_bank()),
        static_cast<unsigned>(b_current.sp_bank()),
        static_cast<unsigned>(d_current.sp_bank()),
        static_cast<unsigned>(next_total_rows));
}

} // namespace smesh
