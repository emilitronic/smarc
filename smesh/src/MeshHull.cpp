// **********************************************************************
// smesh/src/MeshHull.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 2 2026

#include "MeshHull.hpp"

namespace smesh {

void MeshHull::reset() {
  core_.reset();
  a_buf_        = MeshInputRow{};
  b_buf_        = MeshAccumRow{};
  d_buf_        = MeshInputRow{};
  a_skew_       = SkewState<Elem>{};
  b_skew_       = SkewState<Acc>{};
  d_skew_       = SkewState<Elem>{};
  control_skew_ = SkewState<MeshCoreControl>{};
  status_skew_  = SkewState<MeshCoreStatus>{};
  out_b_skew_   = SkewState<Acc>{};
  out_status_skew_ = SkewState<MeshCoreStatus>{};
  in_prop_      = false;
  out_          = MeshHullOut{};
}

void MeshHull::step(const MeshHullIn& in) {
  out_ = MeshHullOut{};

  const auto current_a_buf    = a_buf_;
  const auto current_b_buf    = b_buf_;
  const auto current_d_buf    = d_buf_;
  const auto current_in_prop  = in_prop_;
  auto       next_a_buf       = a_buf_;
  auto       next_b_buf       = b_buf_;
  auto       next_d_buf       = d_buf_;
  auto       next_in_prop     = in_prop_;

  if (in.a_fire != 0) {
    next_a_buf = in.a_is_from_transposer != 0 ? in.transposer_out_col_bits                : in.a_bits;
  }
  if (in.b_fire != 0) {
    next_b_buf = in.b_is_from_transposer != 0 ? widenInputRow(in.transposer_out_col_bits) : widenInputRow(in.b_bits);
  }
  if (in.d_fire != 0) {
    next_d_buf = in.d_is_from_transposer != 0 ? in.transposer_out_col_bits                : in.d_bits;
  }

  if (in.req_fire != 0) {
    next_in_prop = in.pe_control.propagate != 0;
  }

  MeshCoreIn core_in{};
  MeshCoreControlRow control_in{};
  MeshCoreStatusRow status_in{};
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    control_in[lane].prop   = current_in_prop;
    status_in[lane].in_id   = in.matmul_id;
    status_in[lane].in_last = in.last_fire != 0;
    status_in[lane].valid   = in.not_paused != 0;
  }

  core_in.in_a    = stepInputSkew(a_skew_, current_a_buf);
  core_in.in_b    = stepInputSkew(b_skew_, current_b_buf);
  core_in.in_d    = stepInputSkew(d_skew_, current_d_buf);
  core_in.control = stepInputSkew(control_skew_, control_in);
  core_in.status  = stepInputSkew(status_skew_, status_in);

  core_.step(core_in); // update systolic Core state based on the inputs and get new outputs

  a_buf_   = next_a_buf;
  b_buf_   = next_b_buf;
  d_buf_   = next_d_buf;
  in_prop_ = next_in_prop;

  const auto out_b          = stepOutputSkew(out_b_skew_, core_.outB());
  const auto out_status_row = stepOutputSkew(out_status_skew_, core_.outBStatus());
  const auto out_status     = out_status_row[0];
  out_.resp_data     = out_b;
  out_.resp_valid    = bit(out_status.valid);
  out_.resp_last     = bit(out_status.in_last);
  out_.out_matmul_id = out_status.in_id;
}
// Widen element bitdwidth of row input vector to accumulator and hence partial sum bitwidth
MeshAccumRow MeshHull::widenInputRow(const MeshInputRow& row) const {
  MeshAccumRow widened{};
  for (std::size_t i = 0; i < kDim; ++i) {
    widened[i] = static_cast<Acc>(row[i]);
  }
  return widened;
}

} // namespace smesh
