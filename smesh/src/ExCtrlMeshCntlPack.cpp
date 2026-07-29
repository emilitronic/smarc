// **********************************************************************
// smesh/src/ExCtrlMeshCntlPack.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "ExCtrlMeshCntlPack.hpp"

namespace smesh {

ExCtrlMeshCntlPack::ExCtrlMeshCntlPack(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(perform_mul_pre,
             perform_single_mul,
             perform_single_preload,
             a_bank,
             b_bank,
             d_bank,
             a_bank_acc,
             b_bank_acc)
      .reads(d_bank_acc,
             a_read_from_acc,
             b_read_from_acc,
             d_read_from_acc,
             a_garbage,
             b_garbage,
             d_garbage,
             accumulate_zeros)
      .reads(preload_zeros,
             a_fire,
             b_fire,
             d_fire,
             a_unpadded_cols,
             b_unpadded_cols,
             d_unpadded_cols,
             c_addr)
      .reads(c_rows,
             c_cols,
             a_transpose,
             bd_transpose,
             total_rows,
             rs_tag_valid,
             rs_tag,
             dataflow)
      .reads(prop, shift, im2colling, first)
      .writes(cntl);
}

void ExCtrlMeshCntlPack::update() {
  ExCtrlMeshCntl next{};
  next.perform_mul_pre = *perform_mul_pre;
  next.perform_single_mul = *perform_single_mul;
  next.perform_single_preload = *perform_single_preload;
  next.a_bank = static_cast<std::uint32_t>(*a_bank);
  next.b_bank = static_cast<std::uint32_t>(*b_bank);
  next.d_bank = static_cast<std::uint32_t>(*d_bank);
  next.a_bank_acc = static_cast<std::uint32_t>(*a_bank_acc);
  next.b_bank_acc = static_cast<std::uint32_t>(*b_bank_acc);
  next.d_bank_acc = static_cast<std::uint32_t>(*d_bank_acc);
  next.a_read_from_acc = *a_read_from_acc;
  next.b_read_from_acc = *b_read_from_acc;
  next.d_read_from_acc = *d_read_from_acc;
  next.a_garbage = *a_garbage;
  next.b_garbage = *b_garbage;
  next.d_garbage = *d_garbage;
  next.accumulate_zeros = *accumulate_zeros;
  next.preload_zeros = *preload_zeros;
  next.a_fire = *a_fire;
  next.b_fire = *b_fire;
  next.d_fire = *d_fire;
  next.a_unpadded_cols = static_cast<std::uint32_t>(*a_unpadded_cols);
  next.b_unpadded_cols = static_cast<std::uint32_t>(*b_unpadded_cols);
  next.d_unpadded_cols = static_cast<std::uint32_t>(*d_unpadded_cols);
  next.c_addr = *c_addr;
  next.c_rows = static_cast<std::uint32_t>(*c_rows);
  next.c_cols = static_cast<std::uint32_t>(*c_cols);
  next.a_transpose = *a_transpose;
  next.bd_transpose = *bd_transpose;
  next.total_rows = static_cast<std::uint32_t>(*total_rows);
  next.rs_tag_valid = *rs_tag_valid;
  next.rs_tag = *rs_tag;
  next.dataflow = static_cast<std::uint32_t>(*dataflow);
  next.prop = *prop;
  next.shift = static_cast<std::uint32_t>(*shift);
  next.im2colling = *im2colling;
  next.first = *first;
  cntl = next;
}

} // namespace smesh
