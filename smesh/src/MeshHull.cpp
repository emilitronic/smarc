// **********************************************************************
// smesh/src/MeshHull.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 2 2026

#include "MeshHull.hpp"

namespace smesh {

namespace {

std::uint8_t byteAt(u64 data, std::size_t index) {
  return static_cast<std::uint8_t>((data >> (8 * index)) & 0xffu);
}

} // namespace

void MeshHull::reset() {
  core_.reset();
  out_ = MeshHullOut{};
}

void MeshHull::step(const MeshHullIn& in) {
  out_ = MeshHullOut{};
  if (in.pause != 0) {
    return;
  }

  const u64 a_data = in.a_is_from_transposer != 0 ? in.transposer_out_col_bits : static_cast<u64>(in.a_bits.data);
  const u64 b_data = in.b_is_from_transposer != 0 ? in.transposer_out_col_bits : static_cast<u64>(in.b_bits.data);
  const u64 d_data = in.d_is_from_transposer != 0 ? in.transposer_out_col_bits : static_cast<u64>(in.d_bits.data);

  MeshCoreIn core_in{};
  if (in.a_fire != 0 && in.a_bits.valid != 0) {
    core_in.in_a = inputRowFromBits(a_data);
  }
  if (in.b_fire != 0 && in.b_bits.valid != 0) {
    core_in.in_b = accumRowFromBits(b_data);
  }
  if (in.d_fire != 0 && in.d_bits.valid != 0) {
    core_in.in_d = inputRowFromBits(d_data);
  }
  core_in.control.in_id   = in.matmul_id;
  core_in.control.in_last = in.last_fire != 0;
  core_in.control.prop    = in.pe_control.propagate != 0;
  core_in.control.valid   = in.a_fire != 0 || in.b_fire != 0 || in.d_fire != 0 || in.req_fire != 0;

  core_.step(core_in);

  const auto& out_control = core_.outBControl()[0];
  out_.resp_data     = core_.outB();
  out_.resp_valid    = bit(out_control.valid);
  out_.resp_last     = bit(out_control.in_last);
  out_.out_matmul_id = out_control.in_id;
}

MeshInputRow MeshHull::inputRowFromBits(u64 data) const {
  MeshInputRow row{};
  for (std::size_t i = 0; i < kDim && i < sizeof(data); ++i) {
    row[i] = static_cast<Elem>(byteAt(data, i));
  }
  return row;
}

MeshAccumRow MeshHull::accumRowFromBits(u64 data) const {
  MeshAccumRow row{};
  for (std::size_t i = 0; i < kDim; ++i) {
    row[i] = static_cast<Acc>(byteAt(data, i));
  }
  return row;
}

} // namespace smesh
