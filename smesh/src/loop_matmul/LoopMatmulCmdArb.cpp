// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulCmdArb.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulCmdArb.hpp"

namespace smesh {

LoopMatmulCmdArb::LoopMatmulCmdArb(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateOutput)
      .reads(st_c_val, st_c_bits, ex_val, ex_bits, ld_d_val, ld_d_bits,
             ld_ab_val, ld_ab_bits)
      .reads(st_c_spad_val, st_c_spad_bits)
      .writes(selected_, out_val, out_bits);
  UPDATE(updateReady).reads(selected_, out_rdy)
      .writes(st_c_rdy, ex_rdy, ld_d_rdy, ld_ab_rdy, st_c_spad_rdy);
}

void LoopMatmulCmdArb::updateOutput() {
  selected_ = 5;
  out_val = 0;
  out_bits = SmeshCmd{};

  if (st_c_val == 1) {
    selected_ = 0;
    out_val = 1;
    out_bits = *st_c_bits;
  } else if (ex_val == 1) {
    selected_ = 1;
    out_val = 1;
    out_bits = *ex_bits;
  } else if (ld_d_val == 1) {
    selected_ = 2;
    out_val = 1;
    out_bits = *ld_d_bits;
  } else if (ld_ab_val == 1) {
    selected_ = 3;
    out_val = 1;
    out_bits = *ld_ab_bits;
  } else if (st_c_spad_val == 1) {
    selected_ = 4;
    out_val = 1;
    out_bits = *st_c_spad_bits;
  }
}

void LoopMatmulCmdArb::updateReady() {
  st_c_rdy = bit(selected_ == 0 && out_rdy == 1);
  ex_rdy = bit(selected_ == 1 && out_rdy == 1);
  ld_d_rdy = bit(selected_ == 2 && out_rdy == 1);
  ld_ab_rdy = bit(selected_ == 3 && out_rdy == 1);
  st_c_spad_rdy = bit(selected_ == 4 && out_rdy == 1);
}

void LoopMatmulCmdArb::reset() {
  st_c_rdy.reset(0);
  ex_rdy.reset(0);
  ld_d_rdy.reset(0);
  ld_ab_rdy.reset(0);
  st_c_spad_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshCmd{});
  selected_.reset(5);
}

} // namespace smesh
