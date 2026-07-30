// **********************************************************************
// smesh/src/Mesher.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "Mesher.hpp"

namespace smesh {

Mesher::Mesher(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(req_val,
             req_bits,
             a_val,
             a_bits,
             b_val,
             b_bits,
             d_val,
             d_bits)
      .reads(resp_rdy)
      .writes(req_rdy,
              a_rdy,
              b_rdy,
              d_rdy,
              resp_val,
              resp_bits,
              tags_in_progress);
}

void Mesher::update() {
  req_rdy = 1;
  a_rdy = 1;
  b_rdy = 1;
  d_rdy = 1;

  resp_val = 0;
  resp_bits = MesherResp{};

  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    tags_in_progress[i] = MesherTag{};
  }
}

void Mesher::reset() {
  req_rdy.reset(0);
  a_rdy.reset(0);
  b_rdy.reset(0);
  d_rdy.reset(0);
  resp_val.reset(0);
  resp_bits.reset(MesherResp{});

  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    tags_in_progress[i].reset(MesherTag{});
  }
}

} // namespace smesh
