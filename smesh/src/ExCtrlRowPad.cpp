// **********************************************************************
// smesh/src/ExCtrlRowPad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 15 2026

#include "ExCtrlRowPad.hpp"

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

  a_unpadded_cols = a_real ? static_cast<std::uint32_t>(*a_cols) : 0u;
  b_unpadded_cols = b_real ? static_cast<std::uint32_t>(*b_cols) : 0u;
  d_unpadded_cols = d_real ? static_cast<std::uint32_t>(*d_cols) : 0u;
}

} // namespace smesh
