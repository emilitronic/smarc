// **********************************************************************
// smesh/src/StCtrlGeom.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026

#include "StCtrlGeom.hpp"

namespace smesh {

namespace {

SmeshLocalAddr garbageAddr() {
  return SmeshLocalAddr{
      kLocalAddrIsAccMask |
      kLocalAddrAccumulateMask |
      kLocalAddrReadFullAccRowMask |
      kLocalAddrGarbageMask |
      kLocalAddrDataMask};
}

} // namespace

StCtrlGeom::StCtrlGeom(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(vaddr,
             localaddr,
             dst_spad_addr,
             dst_spad_stride,
             stride,
             pool_stride,
             pool_size,
             pool_out_dim)
      .reads(pool_porows,
             pool_pocols,
             pool_orows,
             pool_ocols,
             pool_upad,
             pool_lpad,
             row_counter,
             block_counter)
      .reads(porow_counter,
             pocol_counter,
             wrow_counter,
             wcol_counter)
      .writes(pooling_is_enabled,
              mvout_1d_enabled,
              orow,
              ocol,
              orow_is_negative,
              ocol_is_negative,
              pool_total_rows,
              mvout_1d_rows)
      .writes(current_vaddr,
              current_localaddr,
              current_dst_spad_addr,
              pool_row_addr,
              pool_vaddr);
}

void StCtrlGeom::update() {
  const auto base_vaddr = static_cast<std::uint64_t>(*vaddr);
  const auto base_localaddr = *localaddr;
  const auto base_dst_spad_addr = *dst_spad_addr;
  const auto row = static_cast<std::uint32_t>(*row_counter);
  const auto block = static_cast<std::uint32_t>(*block_counter);
  const auto row_stride = static_cast<std::uint32_t>(*stride);
  const auto dst_stride = static_cast<std::uint32_t>(*dst_spad_stride);

  const auto pstride = static_cast<std::uint32_t>(*pool_stride);
  const auto psize = static_cast<std::uint32_t>(*pool_size);
  const auto pout_dim = static_cast<std::uint32_t>(*pool_out_dim);
  const auto porows = static_cast<std::uint32_t>(*pool_porows);
  const auto pocols = static_cast<std::uint32_t>(*pool_pocols);
  const auto source_orows = static_cast<std::uint32_t>(*pool_orows);
  const auto source_ocols = static_cast<std::uint32_t>(*pool_ocols);
  const auto upad = static_cast<std::uint32_t>(*pool_upad);
  const auto lpad = static_cast<std::uint32_t>(*pool_lpad);

  const auto porow = static_cast<std::uint32_t>(*porow_counter);
  const auto pocol = static_cast<std::uint32_t>(*pocol_counter);
  const auto wrow = static_cast<std::uint32_t>(*wrow_counter);
  const auto wcol = static_cast<std::uint32_t>(*wcol_counter);

  const bool pooling = kHasMaxPool && pstride != 0; // does it include max pooling support?
  const bool moveout_1d = psize != 0 && !pooling;
  const std::uint32_t orow_origin = porow * pstride + wrow;
  const std::uint32_t ocol_origin = pocol * pstride + wcol;
  const bool row_negative = orow_origin < upad;
  const bool col_negative = ocol_origin < lpad;
  const std::uint32_t next_orow = orow_origin - upad;
  const std::uint32_t next_ocol = ocol_origin - lpad;
  const bool pool_element_is_padding =
      row_negative || col_negative ||
      next_orow >= source_orows || next_ocol >= source_ocols;

  pooling_is_enabled = bit(pooling);
  mvout_1d_enabled = bit(moveout_1d);
  orow = next_orow;
  ocol = next_ocol;
  orow_is_negative = bit(row_negative);
  ocol_is_negative = bit(col_negative);
  pool_total_rows = porows * pocols * psize * psize;
  mvout_1d_rows = source_orows * source_ocols;

  current_vaddr = base_vaddr + static_cast<std::uint64_t>(row) * row_stride;
  current_localaddr = base_localaddr + (block * static_cast<std::uint32_t>(kDim) + row);
  current_dst_spad_addr =
      static_cast<std::uint64_t>(base_dst_spad_addr.raw) +
      static_cast<std::uint64_t>(row) * dst_stride;
  pool_row_addr = pool_element_is_padding
                      ? garbageAddr()
                      : base_localaddr + (next_orow * source_ocols + next_ocol);
  pool_vaddr = base_vaddr +
               static_cast<std::uint64_t>(porow * pout_dim + pocol) * row_stride;
}

void StCtrlGeom::reset() {
  pooling_is_enabled.reset(0);
  mvout_1d_enabled.reset(0);
  orow.reset(0);
  ocol.reset(0);
  orow_is_negative.reset(0);
  ocol_is_negative.reset(0);
  pool_total_rows.reset(0);
  mvout_1d_rows.reset(0);
  current_vaddr.reset(0);
  current_localaddr.reset(SmeshLocalAddr{});
  current_dst_spad_addr.reset(0);
  pool_row_addr.reset(SmeshLocalAddr{});
  pool_vaddr.reset(0);
}

} // namespace smesh
