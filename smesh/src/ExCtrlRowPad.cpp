// **********************************************************************
// smesh/src/ExCtrlRowPad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 15 2026

#include "ExCtrlRowPad.hpp"

#include <cstdint>

namespace smesh {

TraceKey(ex_ctrl_row_pad_view);

namespace {

std::uint32_t reverseRowOffset(std::uint32_t block_size, std::uint32_t counter) {
  if (block_size == 0 || counter >= block_size) {
    return 0;
  }
  return block_size - 1 - counter;
}

} // namespace

ExCtrlRowPad::ExCtrlRowPad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_fire_counter,
             b_fire_counter,
             d_fire_counter,
             a_rows,
             b_rows,
             d_rows)
      .reads(a_cols,
             b_cols,
             d_cols,
             block_size)
      .writes(a_row_is_not_all_zeros,
              b_row_is_not_all_zeros,
              d_row_is_not_all_zeros,
              a_unpadded_cols,
              b_unpadded_cols,
              d_unpadded_cols);
}

void ExCtrlRowPad::update() {
  const auto a_counter = static_cast<std::uint32_t>(*a_fire_counter);
  const auto b_counter = static_cast<std::uint32_t>(*b_fire_counter);
  const auto d_counter = static_cast<std::uint32_t>(*d_fire_counter);
  const auto d_row = reverseRowOffset(static_cast<std::uint32_t>(*block_size), d_counter);

  const bool a_real = a_counter < static_cast<std::uint32_t>(*a_rows);
  const bool b_real = b_counter < static_cast<std::uint32_t>(*b_rows);
  const bool d_real = d_row < static_cast<std::uint32_t>(*d_rows);

  a_row_is_not_all_zeros = bit(a_real);
  b_row_is_not_all_zeros = bit(b_real);
  d_row_is_not_all_zeros = bit(d_real);

  const std::uint32_t next_a_unpadded_cols = a_real ? static_cast<std::uint32_t>(*a_cols) : 0u;
  const std::uint32_t next_b_unpadded_cols = b_real ? static_cast<std::uint32_t>(*b_cols) : 0u;
  const std::uint32_t next_d_unpadded_cols = d_real ? static_cast<std::uint32_t>(*d_cols) : 0u;
  a_unpadded_cols = next_a_unpadded_cols;
  b_unpadded_cols = next_b_unpadded_cols;
  d_unpadded_cols = next_d_unpadded_cols;

  trace(ex_ctrl_row_pad_view,
        "counter{a=%05u b=%05u d=%05u} rows{a=%05u b=%05u d=%05u} "
        "cols{a=%05u b=%05u d=%05u} drow=%05u "
        "real{a=%u b=%u d=%u} unpadded{a=%05u b=%05u d=%05u}\n",
        static_cast<unsigned>(a_counter),
        static_cast<unsigned>(b_counter),
        static_cast<unsigned>(d_counter),
        static_cast<unsigned>(*a_rows),
        static_cast<unsigned>(*b_rows),
        static_cast<unsigned>(*d_rows),
        static_cast<unsigned>(*a_cols),
        static_cast<unsigned>(*b_cols),
        static_cast<unsigned>(*d_cols),
        static_cast<unsigned>(d_row),
        static_cast<unsigned>(a_real),
        static_cast<unsigned>(b_real),
        static_cast<unsigned>(d_real),
        static_cast<unsigned>(next_a_unpadded_cols),
        static_cast<unsigned>(next_b_unpadded_cols),
        static_cast<unsigned>(next_d_unpadded_cols));
}

} // namespace smesh
