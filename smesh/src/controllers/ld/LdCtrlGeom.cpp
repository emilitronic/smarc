// **********************************************************************
// smesh/src/controllers/ld/LdCtrlGeom.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrlGeom.hpp"

namespace smesh {

LdCtrlGeom::LdCtrlGeom(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(vaddr, localaddr, rows, stride, row_counter)
                .writes(all_zeros, current_vaddr, localaddr_plus_row_counter,
                        actual_rows_read);
}

void LdCtrlGeom::update() {
  const auto base_vaddr = static_cast<std::uint64_t>(*vaddr);
  const auto row = static_cast<std::uint32_t>(*row_counter);
  const auto row_stride = static_cast<std::uint64_t>(*stride);
  const bool zeros = base_vaddr == 0;

  all_zeros = bit(zeros);
  current_vaddr = base_vaddr + row * row_stride;
  // LocalAddr addition changes only the data portion, preserving metadata.
  localaddr_plus_row_counter = *localaddr + row;
  actual_rows_read = row_stride == 0 && !zeros ? 1u : static_cast<std::uint32_t>(*rows);
}

} // namespace smesh
