// **********************************************************************
// smesh/src/MeshHull.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 2 2026

#include "MeshHull.hpp"

namespace smesh {

void MeshHull::reset() {
  core_.reset();
  a_buf_  = MeshInputRow{};
  b_buf_  = MeshAccumRow{};
  d_buf_  = MeshInputRow{};
  a_skew_ = SkewState<Elem>{};
  b_skew_ = SkewState<Acc>{};
  d_skew_ = SkewState<Elem>{};
  control_skew_ = SkewState<MeshCoreControl>{};
  out_    = MeshHullOut{};
}

void MeshHull::step(const MeshHullIn& in) {
  out_ = MeshHullOut{};

  if (in.a_fire != 0) {
    a_buf_ = in.a_is_from_transposer != 0 ? in.transposer_out_col_bits                : in.a_bits;
  }
  if (in.b_fire != 0) {
    b_buf_ = in.b_is_from_transposer != 0 ? widenInputRow(in.transposer_out_col_bits) : widenInputRow(in.b_bits);
  }
  if (in.d_fire != 0) {
    d_buf_ = in.d_is_from_transposer != 0 ? in.transposer_out_col_bits                : in.d_bits;
  }

  MeshCoreIn core_in{};
  MeshCoreControlRow control_in{};
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    control_in[lane].in_id   = in.matmul_id;
    control_in[lane].in_last = in.last_fire != 0;
    control_in[lane].prop    = in.pe_control.propagate != 0;
    control_in[lane].valid   = in.not_paused != 0;
  }

  core_in.in_a            = stepInputSkew(a_skew_, a_buf_);
  core_in.in_b            = stepInputSkew(b_skew_, b_buf_);
  core_in.in_d            = stepInputSkew(d_skew_, d_buf_);
  core_in.control         = stepInputSkew(control_skew_, control_in);

  core_.step(core_in); // update systolic Core state based on the inputs and get new outputs

  const auto& out_control = core_.outBControl()[0]; // acces Core's private control state
  out_.resp_data     = core_.outB();
  out_.resp_valid    = bit(out_control.valid);
  out_.resp_last     = bit(out_control.in_last);
  out_.out_matmul_id = out_control.in_id;
}

MeshAccumRow MeshHull::widenInputRow(const MeshInputRow& row) const {
  MeshAccumRow widened{};
  for (std::size_t i = 0; i < kDim; ++i) {
    widened[i] = static_cast<Acc>(row[i]);
  }
  return widened;
}

} // namespace smesh
