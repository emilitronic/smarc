// **********************************************************************
// smesh/src/ExCtrlMeshInSelPad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "ExCtrlMeshInSelPad.hpp"

#include <cassert>

namespace smesh {

namespace {
// check that a bank index is in range, and convert to std::size_t
std::size_t checkedBankIndex(std::uint32_t index, std::size_t size) {
  assert(size > 0);
  assert(static_cast<std::size_t>(index) < size);
  return static_cast<std::size_t>(index);
}
// TODO: keeping this for now because im2col used u64, but I haven't even implemented im2col
MeshInputRow padInputRow(u64 unpadded, std::uint32_t unpadded_cols) {
  MeshInputRow padded{};
  const std::size_t cols = unpadded_cols < kDim ? unpadded_cols : kDim;
  for (std::size_t lane = 0; lane < cols; ++lane) {
    padded[lane] = static_cast<Elem>((unpadded >> (lane * 8)) & u64{0xff});
  }
  return padded;
}

MeshInputRow padInputRow(const MeshInputRow& unpadded, std::uint32_t unpadded_cols) {
  MeshInputRow padded{};
  const std::size_t cols = unpadded_cols < kDim ? unpadded_cols : kDim;
  for (std::size_t lane = 0; lane < cols; ++lane) {
    padded[lane] = unpadded[lane];
  }
  return padded;
}

} // namespace

ExCtrlMeshInSelPad::ExCtrlMeshInSelPad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(cntl_val,
             cntl_bits,
             mesh_cntl_deq_fire,
             im2col_data,
             im2col_val,
             mesh_a_rdy, mesh_b_rdy, mesh_d_rdy)
      .reads(spad_read_resp_val)
      .reads(accum_read_resp_val, spad_read_resp_data)
      .reads(accum_read_resp_data)
      .writes(mesh_a, mesh_b, mesh_d, mesh_a_val, mesh_b_val, mesh_d_val,
              spad_read_resp_rdy, accum_read_resp_rdy)
      .writes(mesh_a_fire, mesh_b_fire, mesh_d_fire);
}

void ExCtrlMeshInSelPad::update() {
  const auto cntl = *cntl_bits; // deq'd from mesh cntl queue

  // which bank bus (or which mem) to look at
  const auto a_spad_index = checkedBankIndex(cntl.a_bank, kSpBanks);
  const auto b_spad_index = checkedBankIndex(cntl.b_bank, kSpBanks);
  const auto d_spad_index = checkedBankIndex(cntl.d_bank, kSpBanks);
  const auto a_acc_index  = checkedBankIndex(cntl.a_bank_acc, kAccBanks);
  const auto b_acc_index  = checkedBankIndex(cntl.b_bank_acc, kAccBanks);
  const auto d_acc_index  = checkedBankIndex(cntl.d_bank_acc, kAccBanks);
  // data on that bus
  const auto a_spad = spad_read_resp_data[a_spad_index]->data;
  const auto b_spad = spad_read_resp_data[b_spad_index]->data;
  const auto d_spad = spad_read_resp_data[d_spad_index]->data;
  const auto a_acc  = accum_read_resp_data[a_acc_index]->data;
  const auto b_acc  = accum_read_resp_data[b_acc_index]->data;
  const auto d_acc  = accum_read_resp_data[d_acc_index]->data;
  // selected, padded row payloads (*_unpadded_cols are the number of valid elements in the row, before padding)
  const auto next_mesh_a = ExCtrlMeshIn{cntl.a_garbage != 0
      ? MeshInputRow{}
      : cntl.im2colling != 0
          ? padInputRow(*im2col_data, cntl.a_unpadded_cols)
          : cntl.a_read_from_acc != 0
              ? padInputRow(a_acc,  cntl.a_unpadded_cols)
              : padInputRow(a_spad, cntl.a_unpadded_cols)};
  const auto next_mesh_b = ExCtrlMeshIn{(cntl.b_garbage != 0 || cntl.accumulate_zeros != 0)
      ? MeshInputRow{}
      : cntl.b_read_from_acc != 0
          ? padInputRow(b_acc,  cntl.b_unpadded_cols)
          : padInputRow(b_spad, cntl.b_unpadded_cols)};
  const auto next_mesh_d = ExCtrlMeshIn{(cntl.d_garbage != 0 || cntl.preload_zeros != 0)
      ? MeshInputRow{}
      : cntl.d_read_from_acc != 0
          ? padInputRow(d_acc,  cntl.d_unpadded_cols)
          : padInputRow(d_spad, cntl.d_unpadded_cols)};

  // validity of data on the bus (or in memory) for this row-beat
  const bool dataA_valid = cntl.a_garbage != 0 || cntl.a_unpadded_cols == 0 || (cntl.im2colling       != 0 ? im2col_val != 0 : cntl.a_read_from_acc != 0 ? accum_read_resp_val[a_acc_index] != 0 : spad_read_resp_val[a_spad_index] != 0);
  const bool dataB_valid = cntl.b_garbage != 0 || cntl.b_unpadded_cols == 0 || (cntl.accumulate_zeros != 0 ? false           : cntl.b_read_from_acc != 0 ? accum_read_resp_val[b_acc_index] != 0 : spad_read_resp_val[b_spad_index] != 0);
  const bool dataD_valid = cntl.d_garbage != 0 || cntl.d_unpadded_cols == 0 || (cntl.preload_zeros    != 0 ? false           : cntl.d_read_from_acc != 0 ? accum_read_resp_val[d_acc_index] != 0 : spad_read_resp_val[d_spad_index] != 0);

  const auto next_mesh_a_val = bit(cntl_val != 0 && cntl.a_fire != 0 && dataA_valid);
  const auto next_mesh_b_val = bit(cntl_val != 0 && cntl.b_fire != 0 && dataB_valid);
  const auto next_mesh_d_val = bit(cntl_val != 0 && cntl.d_fire != 0 && dataD_valid);

  mesh_a = next_mesh_a;
  mesh_b = next_mesh_b;
  mesh_d = next_mesh_d;
  mesh_a_val = next_mesh_a_val;
  mesh_b_val = next_mesh_b_val;
  mesh_d_val = next_mesh_d_val;
  mesh_a_fire = bit(static_cast<bool>(next_mesh_a_val) && mesh_a_rdy != 0);
  mesh_b_fire = bit(static_cast<bool>(next_mesh_b_val) && mesh_b_rdy != 0);
  mesh_d_fire = bit(static_cast<bool>(next_mesh_d_val) && mesh_d_rdy != 0);

  // Consume only execute responses after the control packet and selected mesh
  // input have both fired. DMA responses belong to the load/store paths.
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_read_resp_rdy[bank] = 0;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_read_resp_rdy[bank] = 0;
  }

  // logic for sending ready signals back to read resp ports of accum and spad
  if (mesh_cntl_deq_fire != 0) {
    if (cntl.a_fire != 0 && mesh_a_fire != 0 && cntl.a_garbage == 0 &&
        cntl.a_unpadded_cols > 0 && cntl.im2colling == 0) {
      if (cntl.a_read_from_acc != 0) {
        accum_read_resp_rdy[a_acc_index] = bit(
            accum_read_resp_data[a_acc_index]->from_dma == 0);
      } else {
        spad_read_resp_rdy[a_spad_index] = bit(
            spad_read_resp_data[a_spad_index]->from_dma == 0);
      }
    }

    if (cntl.b_fire != 0 && mesh_b_fire != 0 && cntl.b_garbage == 0 &&
        cntl.accumulate_zeros == 0 && cntl.b_unpadded_cols > 0) {
      if (cntl.b_read_from_acc != 0) {
        accum_read_resp_rdy[b_acc_index] = bit(
            accum_read_resp_data[b_acc_index]->from_dma == 0);
      } else {
        spad_read_resp_rdy[b_spad_index] = bit(
            spad_read_resp_data[b_spad_index]->from_dma == 0);
      }
    }

    if (cntl.d_fire != 0 && mesh_d_fire != 0 && cntl.d_garbage == 0 &&
        cntl.preload_zeros == 0 && cntl.d_unpadded_cols > 0) {
      if (cntl.d_read_from_acc != 0) {
        accum_read_resp_rdy[d_acc_index] = bit(
            accum_read_resp_data[d_acc_index]->from_dma == 0);
      } else {
        spad_read_resp_rdy[d_spad_index] = bit(
            spad_read_resp_data[d_spad_index]->from_dma == 0);
      }
    }
  }
}

} // namespace smesh
