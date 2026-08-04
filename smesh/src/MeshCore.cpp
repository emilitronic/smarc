// **********************************************************************
// smesh/src/MeshCore.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 1 2026

#include "MeshCore.hpp"

namespace smesh {

void MeshCore::reset() {
  c1_            = InputGrid{};
  c2_            = InputGrid{};
  a_path_        = InputGrid{};
  b_path_        = AccumGrid{};
  d_path_        = InputGrid{};
  control_path_  = CtrlGrid{};
  status_path_   = StatusGrid{};
  out_b_         = MeshAccumRow{};
  out_b_control_ = CtrlRow{};
  out_b_status_  = StatusRow{};
}

void MeshCore::step(const MeshCoreIn& in) {
  InputGrid    next_a{};
  InputGrid    next_c1 = c1_;
  InputGrid    next_c2 = c2_;
  AccumGrid    next_b = b_path_;    // working copy of B/out_b state
  InputGrid    next_d = d_path_;    // working copy of D/out_c state (not used for WS)
  CtrlGrid     next_control_path{};
  StatusGrid   next_status_path{};
  MeshAccumRow next_out_b{};
  CtrlRow      next_out_b_control{};
  StatusRow    next_out_b_status{};

  // A always flows LR; B/out_b and D/out_c flow TB under their aligned valid status.
  for (std::size_t row = 0; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      // what is inside PE[row][col] this cycle?
      const auto a       = col == 0     ? in.in_a[row]    : a_path_[row][col - 1];       // A flows LR
      const auto b_in    = row == 0     ? in.in_b[col]    : b_path_[row - 1][col];       // B flows TB
      const auto d_in    = row == 0     ? in.in_d[col]    : d_path_[row - 1][col];       // D flows TB
      const auto control = row == 0     ? in.control[col] : control_path_[row - 1][col]; // ctrl flows TB
      const auto status  = row == 0     ? in.status[col]  : status_path_[row - 1][col];  // id/last/valid flows TB
      const auto weight  = control.prop ? c2_[row][col]   : c1_[row][col];               // weight=c2 if prop=1, c1 if prop=0

      next_a[row][col] = a; // A that's leaving this PE and going LR

      if (status.valid) {  // if PE's signal is valid...
        next_b[row][col] = b_in + static_cast<Acc>(a) * static_cast<Acc>(weight); // ...update B/out_b for PE below
      }

      if (status.valid) { // if PE's signal is valid...
        if (control.prop) {
          next_d[row][col]  = c1_[row][col]; // ...send old c1 downward on D/out_c
          next_c1[row][col] = d_in; // ...update c1 for this PE if prop=1
        } else {
          next_d[row][col]  = c2_[row][col]; // ...send old c2 downward on D/out_c
          next_c2[row][col] = d_in; // ...update c2 for this PE if prop=0
        }
      }

      next_control_path[row][col] = control; // control that's leaving this PE and going TB
      next_status_path[row][col]  = status;  // status that's leaving this PE and going TB
    }
  }

  // compute out_b and status/ctrl emerging from bottom row
  for (std::size_t col = 0; col < kDim; ++col) {
    next_out_b[col]         = next_b[kDim - 1][col];
    next_out_b_control[col] = next_control_path[kDim - 1][col];
    next_out_b_status[col]  = next_status_path[kDim - 1][col];
  }

  // update state
  c1_            = next_c1;
  c2_            = next_c2;
  a_path_        = next_a;
  b_path_        = next_b;
  d_path_        = next_d;
  control_path_  = next_control_path;
  status_path_   = next_status_path;
  out_b_         = next_out_b;
  out_b_control_ = next_out_b_control;
  out_b_status_  = next_out_b_status;
}

void MeshCore::loadC2ForTest(const InputGrid& weights) {
  c2_ = weights;
}

} // namespace smesh
