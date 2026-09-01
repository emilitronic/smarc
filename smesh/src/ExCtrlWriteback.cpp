// **********************************************************************
// smesh/src/ExCtrlWriteback.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026

#include "ExCtrlWriteback.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace smesh {

namespace {

constexpr std::uint8_t kActivationRelu = 1;

std::uint32_t maskForColumns(std::uint32_t cols,
                             std::size_t element_bytes,
                             std::uint32_t aligned_to) {
  assert(aligned_to != 0);
  assert(element_bytes % aligned_to == 0);
  const auto mask_bits_per_element = element_bytes / aligned_to;
  assert(kDim * mask_bits_per_element <= 32);

  const auto bounded = std::min<std::uint32_t>(cols, kDim);
  std::uint32_t mask = 0;
  for (std::size_t lane = 0; lane < bounded; ++lane) {
    for (std::size_t bit_index = 0; bit_index < mask_bits_per_element; ++bit_index) {
      mask |= std::uint32_t{1} << (lane * mask_bits_per_element + bit_index);
    }
  }
  return mask;
}

std::int8_t clipToElem(Acc value, std::uint8_t activation) {
  if (activation == kActivationRelu && value < 0) {
    value = 0;
  }
  value = std::max<Acc>(value, -128);
  value = std::min<Acc>(value, 127);
  return static_cast<std::int8_t>(value);
}

MeshInputRow narrowInputRow(const MeshAccumRow& row, std::uint8_t activation) {
  MeshInputRow data{};
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    data[lane] = clipToElem(row[lane], activation);
  }
  return data;
}

} // namespace

ExCtrlWriteback::ExCtrlWriteback(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateView)
      .reads(mesh_resp_val,
             mesh_resp_bits,
             current_dataflow,
             c_addr_stride,
             activation,
             aligned_to,
             ex_write_to_spad,
             ex_write_to_acc)
      .reads(spad_write_rdy,
             accum_write_rdy)
      .writes(spad_write_val,
             spad_write_bits,
             accum_write_val,
             accum_write_bits)
      .writes(mesh_completed_rs_tag_fire,
              completed_val,
              completed_bits);
  UPDATE(updateState)
      .reads(mesh_resp_val, mesh_resp_bits);
}

void ExCtrlWriteback::updateView() {
  const auto& response              = *mesh_resp_bits;
  const auto base_address           = response.tag.addr;
  const auto total_rows             = response.total_rows;
  const auto offset                 = output_counter_ * c_addr_stride;
  const auto output_offset          = current_dataflow == kExDataflowWS
                                      ? offset
                                      : total_rows - 1u - offset;
  const auto write_address          = base_address + output_offset;
  const bool is_accumulator_write   = write_address.is_acc_addr();
  const bool is_garbage_address     = base_address.is_garbage();
  const bool response_is_tracked    = mesh_resp_val != 0 && response.tag.rs_tag_valid != 0;
  const bool write_this_row         = total_rows != 0 && (current_dataflow == kExDataflowWS
                                      ? output_counter_ < response.tag.rows
                                      : total_rows - 1u - output_counter_ < response.tag.rows);
  const bool start_array_outputting = response_is_tracked && !is_garbage_address;
  const auto spad_write_mask        = maskForColumns(response.tag.cols, sizeof(Elem), static_cast<std::uint32_t>(*aligned_to));
  const auto accum_write_mask       = maskForColumns(response.tag.cols, sizeof(Acc), static_cast<std::uint32_t>(*aligned_to));

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_write_val[bank]  = 0;
    spad_write_bits[bank] = SpadBankWriteReq{};

    if (ex_write_to_spad != 0 && start_array_outputting &&
        !is_accumulator_write && !is_garbage_address && write_this_row &&
        write_address.sp_bank() == bank) {
      SpadBankWriteReq write{};
      spad_write_val[bank] = 1;
      write.addr = write_address.sp_row();
      write.data = narrowInputRow(response.data, activation);
      write.mask = spad_write_mask;
      spad_write_bits[bank] = write;
    }
  }

  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_write_val[bank]  = 0;
    accum_write_bits[bank] = AccumBankWriteReq{};

    if (ex_write_to_acc != 0 && start_array_outputting &&
        is_accumulator_write && !is_garbage_address && write_this_row &&
        write_address.acc_bank() == bank) {
      AccumBankWriteReq write{};
      accum_write_val[bank] = 1;
      write.addr = write_address.acc_row();
      write.data = response.data;
      write.acc = bit(write_address.accumulate());
      write.mask = accum_write_mask;
      accum_write_bits[bank] = write;
    }
  }

  const bool mesh_completed = response_is_tracked && response.last;
  mesh_completed_rs_tag_fire = bit(mesh_completed);
  completed_val              = bit(mesh_completed);
  completed_bits             = response.tag.rs_tag;
}

void ExCtrlWriteback::updateState() {
  const bool mesh_resp_fire = mesh_resp_val != 0;

  if (mesh_resp_fire && mesh_resp_bits->tag.rs_tag_valid != 0) {
    const auto total_rows = mesh_resp_bits->total_rows;
    if (total_rows == 0) {
      output_counter_ = 0;
    } else {
      output_counter_ = (output_counter_ + 1) % total_rows;
    }

    if (mesh_resp_bits->last) {
      output_counter_ = 0;
    }
  }
}

void ExCtrlWriteback::reset() {
  output_counter_ = 0;

  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    spad_write_val[bank].reset(0);
    spad_write_bits[bank].reset(SpadBankWriteReq{});
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    accum_write_val[bank].reset(0);
    accum_write_bits[bank].reset(AccumBankWriteReq{});
  }
  mesh_completed_rs_tag_fire.reset(0);
  completed_val.reset(0);
  completed_bits.reset(0);
}

} // namespace smesh
