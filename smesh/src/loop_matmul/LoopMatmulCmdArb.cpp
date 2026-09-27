// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulCmdArb.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026

#include "LoopMatmulCmdArb.hpp"

namespace smesh {

LoopMatmulCmdArb::LoopMatmulCmdArb(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(st_c_val, st_c_bits, ex_val, ex_bits, ld_d_val, ld_d_bits,
             ld_ab_val, ld_ab_bits)
      .reads(st_c_spad_val, st_c_spad_bits, out_rdy)
      .writes(st_c_rdy, ex_rdy, ld_d_rdy, ld_ab_rdy, st_c_spad_rdy,
              out_val, out_bits);
}

void LoopMatmulCmdArb::update() {
  st_c_rdy = 0;
  ex_rdy = 0;
  ld_d_rdy = 0;
  ld_ab_rdy = 0;
  st_c_spad_rdy = 0;
  out_val = 0;
  out_bits = SmeshCmd{};

  if (st_c_val == 1) {
    out_val = 1;
    out_bits = *st_c_bits;
    st_c_rdy = *out_rdy;
  } else if (ex_val == 1) {
    out_val = 1;
    out_bits = *ex_bits;
    ex_rdy = *out_rdy;
  } else if (ld_d_val == 1) {
    out_val = 1;
    out_bits = *ld_d_bits;
    ld_d_rdy = *out_rdy;
  } else if (ld_ab_val == 1) {
    out_val = 1;
    out_bits = *ld_ab_bits;
    ld_ab_rdy = *out_rdy;
  } else if (st_c_spad_val == 1) {
    out_val = 1;
    out_bits = *st_c_spad_bits;
    st_c_spad_rdy = *out_rdy;
  }
}

void LoopMatmulCmdArb::reset() {
  st_c_rdy.reset(0);
  ex_rdy.reset(0);
  ld_d_rdy.reset(0);
  ld_ab_rdy.reset(0);
  st_c_spad_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshCmd{});
}

} // namespace smesh
