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
  a_buf_ = MeshInputRow{};
  b_buf_ = MeshAccumRow{};
  d_buf_ = MeshInputRow{};
  out_   = MeshHullOut{};
}

void MeshHull::step(const MeshHullIn& in) {
  out_ = MeshHullOut{};

  if (in.a_fire != 0) {
    a_buf_ = in.a_is_from_transposer != 0 ? inputRowFromBits(in.transposer_out_col_bits) : in.a_bits;
  }
  if (in.b_fire != 0) {
    b_buf_ = in.b_is_from_transposer != 0 ? widenInputRow(inputRowFromBits(in.transposer_out_col_bits)) : widenInputRow(in.b_bits);
  }
  if (in.d_fire != 0) {
    d_buf_ = in.d_is_from_transposer != 0 ? inputRowFromBits(in.transposer_out_col_bits) : in.d_bits;
  }

  MeshCoreIn core_in{};
  core_in.in_a = a_buf_;
  core_in.in_b = b_buf_;
  core_in.in_d = d_buf_;
  core_in.control.in_id   = in.matmul_id;
  core_in.control.in_last = in.last_fire != 0;
  core_in.control.prop    = in.pe_control.propagate != 0;
  core_in.control.valid   = in.pause == 0;

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

MeshAccumRow MeshHull::widenInputRow(const MeshInputRow& row) const {
  MeshAccumRow widened{};
  for (std::size_t i = 0; i < kDim; ++i) {
    widened[i] = static_cast<Acc>(row[i]);
  }
  return widened;
}

} // namespace smesh
