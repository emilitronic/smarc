// **********************************************************************
// smesh/src/MeshCore.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 1 2026

#include "MeshCore.hpp"

namespace smesh {

void MeshCore::reset() {
  c1_         = InputGrid{};
  c2_         = InputGrid{};
  a_path_     = InputGrid{};
  b_path_     = AccumGrid{};
  b_ctrl_     = CtrlGrid{};
  d_path_     = InputGrid{};
  d_ctrl_     = CtrlGrid{};
  out_b_      = MeshAccumRow{};
  out_b_ctrl_ = CtrlRow{};
}

// preload mechanics (weights go through D path)
void MeshCore::stepPreloadD(const MeshInputRow& d_shifter_in, MeshCoreCtrl ctrl) {
  InputGrid next_d = d_path_; // working copy of D path state (updating weights)
  CtrlGrid  next_d_ctrl{};
  // shift new D & valid TB into systolic, but only shift in D if valid is true
  for (std::size_t col = 0; col < kDim; ++col) {
    if (ctrl.valid) {
      next_d[0][col] = d_shifter_in[col];
    }
    next_d_ctrl[0][col] = ctrl;
  }
  // shift D & valid TB through systolic, but only shift D if corresponding valid is true
  for (std::size_t row = 1; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      if (d_ctrl_[row - 1][col].valid) {
        next_d[row][col] = d_path_[row - 1][col];
      }
      next_d_ctrl[row][col] = d_ctrl_[row - 1][col];
    }
  }
  // update state
  d_path_  = next_d;
  d_ctrl_  = next_d_ctrl;
  // store weights into WS stationary registers if valid is true
  for (std::size_t row = 0; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      const auto& d_ctrl = d_ctrl_[row][col];
      if (d_ctrl.valid) {
        if (d_ctrl.prop) {
          c1_[row][col] = d_path_[row][col]; // weight goes into c1 when prop is true
        } else {
          c2_[row][col] = d_path_[row][col];
        }
      }
    }
  }
}

// WS compute mechanics (A goes through A path, B/out_b goes through B path)
void MeshCore::stepWsCompute(const MeshInputRow& a_shifter_in, const MeshAccumRow& b_shifter_in, MeshCoreCtrl ctrl) {
  InputGrid    next_a{};
  AccumGrid    next_b = b_path_; // working copy of B path state (updating psums/results)
  CtrlGrid     next_b_ctrl{};
  MeshAccumRow next_out_b{};
  CtrlRow      next_out_b_ctrl{};

  // what's going into next registers, and what's actually getting latched by them, and what's computed
  for (std::size_t row = 0; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      // what's going into next registers
      const auto a         = col == 0       ? a_shifter_in[row] : a_path_[row][col - 1]; // A moves LR
      const auto b_in      = row == 0       ? b_shifter_in[col] : b_path_[row - 1][col]; // B/out_b moves TB
      const auto b_in_ctrl = row == 0       ? ctrl              : b_ctrl_[row - 1][col]; // control moves TB
      const auto weight    = b_in_ctrl.prop ? c2_[row][col]     : c1_[row][col]; // weight comes from c2 when prop is true

      next_a[row][col] = a; // latch a
      if (b_in_ctrl.valid) { // latch b_in and compute new psum if valid is true
        next_b[row][col] = b_in + static_cast<Acc>(a) * static_cast<Acc>(weight);
      }
      next_b_ctrl[row][col] = b_in_ctrl; // latch aligned control
    }
  }

  // compute out_b and status/ctrl emerging from bottom row
  for (std::size_t col = 0; col < kDim; ++col) {
    next_out_b[col]      = next_b[kDim - 1][col];
    next_out_b_ctrl[col] = next_b_ctrl[kDim - 1][col];
  }
 
  // update state
  a_path_ = next_a;
  b_path_ = next_b;
  b_ctrl_ = next_b_ctrl;
  out_b_ = next_out_b;
  out_b_ctrl_ = next_out_b_ctrl;
}

} // namespace smesh
