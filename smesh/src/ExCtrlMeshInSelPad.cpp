// **********************************************************************
// smesh/src/ExCtrlMeshInSelPad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "ExCtrlMeshInSelPad.hpp"

namespace smesh {

namespace {

std::size_t boundedIndex(std::uint32_t index, std::size_t size) {
  return size == 0 ? 0 : static_cast<std::size_t>(index) % size;
}

u64 padInputRow(u64 unpadded, std::uint32_t unpadded_cols) {
  u64 padded = 0;
  const std::size_t cols = unpadded_cols < kDim ? unpadded_cols : kDim;
  for (std::size_t lane = 0; lane < cols; ++lane) {
    padded |= unpadded & (u64{0xff} << (lane * 8));
  }
  return padded;
}

} // namespace

ExCtrlMeshInSelPad::ExCtrlMeshInSelPad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_bank,
             b_bank,
             d_bank,
             a_bank_acc,
             b_bank_acc,
             d_bank_acc,
             a_read_from_acc,
             b_read_from_acc)
      .reads(d_read_from_acc,
             a_garbage,
             b_garbage,
             d_garbage,
             accumulate_zeros,
             preload_zeros,
             im2colling,
             im2col_data)
      .reads(a_unpadded_cols,
             b_unpadded_cols,
             d_unpadded_cols,
             a_fire,
             b_fire,
             d_fire,
             spad_read_data)
      .reads(accum_read_data)
      .writes(mesh_a, mesh_b, mesh_d);
}

void ExCtrlMeshInSelPad::update() {
  const auto a_spad = spad_read_data[boundedIndex(a_bank, kSpBanks)]->data;
  const auto b_spad = spad_read_data[boundedIndex(b_bank, kSpBanks)]->data;
  const auto d_spad = spad_read_data[boundedIndex(d_bank, kSpBanks)]->data;
  const auto a_acc = accum_read_data[boundedIndex(a_bank_acc, kAccBanks)]->data;
  const auto b_acc = accum_read_data[boundedIndex(b_bank_acc, kAccBanks)]->data;
  const auto d_acc = accum_read_data[boundedIndex(d_bank_acc, kAccBanks)]->data;

  const auto a_unpadded =
      a_garbage != 0 ? u64{0} :
      im2colling != 0 ? *im2col_data :
      a_read_from_acc != 0 ? a_acc :
      a_spad;
  const auto b_unpadded =
      (b_garbage != 0 || accumulate_zeros != 0) ? u64{0} :
      b_read_from_acc != 0 ? b_acc :
      b_spad;
  const auto d_unpadded =
      (d_garbage != 0 || preload_zeros != 0) ? u64{0} :
      d_read_from_acc != 0 ? d_acc :
      d_spad;

  mesh_a = ExCtrlMeshInput{padInputRow(a_unpadded, a_unpadded_cols), bit(a_fire != 0)};
  mesh_b = ExCtrlMeshInput{padInputRow(b_unpadded, b_unpadded_cols), bit(b_fire != 0)};
  mesh_d = ExCtrlMeshInput{padInputRow(d_unpadded, d_unpadded_cols), bit(d_fire != 0)};
}

} // namespace smesh
