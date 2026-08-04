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
      .reads(cntl_val,
             a_bank,
             b_bank,
             d_bank,
             a_bank_acc,
             b_bank_acc,
             d_bank_acc,
             a_read_from_acc)
      .reads(b_read_from_acc,
             d_read_from_acc,
             a_garbage,
             b_garbage,
             d_garbage,
             accumulate_zeros,
             preload_zeros,
             im2colling)
      .reads(im2col_data,
             im2col_val,
             a_unpadded_cols,
             b_unpadded_cols,
             d_unpadded_cols,
             cntl_a_fire,
             cntl_b_fire,
             cntl_d_fire)
      .reads(mesh_a_rdy,
             mesh_b_rdy,
             mesh_d_rdy,
             spad_read_val)
      .reads(accum_read_val, spad_read_data)
      .reads(accum_read_data)
      .writes(mesh_a, mesh_b, mesh_d, mesh_a_val, mesh_b_val, mesh_d_val)
      .writes(mesh_a_fire, mesh_b_fire, mesh_d_fire);
}

void ExCtrlMeshInSelPad::update() {
  const auto a_spad_index = boundedIndex(a_bank, kSpBanks);
  const auto b_spad_index = boundedIndex(b_bank, kSpBanks);
  const auto d_spad_index = boundedIndex(d_bank, kSpBanks);
  const auto a_acc_index = boundedIndex(a_bank_acc, kAccBanks);
  const auto b_acc_index = boundedIndex(b_bank_acc, kAccBanks);
  const auto d_acc_index = boundedIndex(d_bank_acc, kAccBanks);

  const auto a_spad = spad_read_data[a_spad_index]->data;
  const auto b_spad = spad_read_data[b_spad_index]->data;
  const auto d_spad = spad_read_data[d_spad_index]->data;
  const auto a_acc = accum_read_data[a_acc_index]->data;
  const auto b_acc = accum_read_data[b_acc_index]->data;
  const auto d_acc = accum_read_data[d_acc_index]->data;

  const auto a_unpadded = a_garbage != 0 ? u64{0} : im2colling != 0 ? *im2col_data : a_read_from_acc != 0 ? a_acc : a_spad;
  const auto b_unpadded = (b_garbage != 0 || accumulate_zeros != 0) ? u64{0} : b_read_from_acc != 0 ? b_acc : b_spad;
  const auto d_unpadded = (d_garbage != 0 || preload_zeros != 0) ? u64{0} : d_read_from_acc != 0 ? d_acc : d_spad;

  const bool dataA_valid = a_garbage != 0 || a_unpadded_cols == 0 || (im2colling != 0 ? im2col_val != 0 :  a_read_from_acc != 0 ? accum_read_val[a_acc_index] != 0 : spad_read_val[a_spad_index] != 0);
  const bool dataB_valid = b_garbage != 0 || b_unpadded_cols == 0 || (accumulate_zeros != 0 ? false : b_read_from_acc != 0 ? accum_read_val[b_acc_index] != 0 : spad_read_val[b_spad_index] != 0);
  const bool dataD_valid = d_garbage != 0 || d_unpadded_cols == 0 || (preload_zeros != 0 ? false : d_read_from_acc != 0 ? accum_read_val[d_acc_index] != 0 : spad_read_val[d_spad_index] != 0);

  const auto next_mesh_a_val = bit(cntl_val != 0 && cntl_a_fire != 0 && dataA_valid);
  const auto next_mesh_b_val = bit(cntl_val != 0 && cntl_b_fire != 0 && dataB_valid);
  const auto next_mesh_d_val = bit(cntl_val != 0 && cntl_d_fire != 0 && dataD_valid);
  const auto next_mesh_a = ExCtrlMeshIn{padInputRow(a_unpadded, a_unpadded_cols)};
  const auto next_mesh_b = ExCtrlMeshIn{padInputRow(b_unpadded, b_unpadded_cols)};
  const auto next_mesh_d = ExCtrlMeshIn{padInputRow(d_unpadded, d_unpadded_cols)};

  mesh_a = next_mesh_a;
  mesh_b = next_mesh_b;
  mesh_d = next_mesh_d;
  mesh_a_val = next_mesh_a_val;
  mesh_b_val = next_mesh_b_val;
  mesh_d_val = next_mesh_d_val;
  mesh_a_fire = bit(static_cast<bool>(next_mesh_a_val) && mesh_a_rdy != 0);
  mesh_b_fire = bit(static_cast<bool>(next_mesh_b_val) && mesh_b_rdy != 0);
  mesh_d_fire = bit(static_cast<bool>(next_mesh_d_val) && mesh_d_rdy != 0);
}

} // namespace smesh
