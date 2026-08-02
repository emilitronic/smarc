// **********************************************************************
// smesh/src/MeshCore.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 1 2026

#include "MeshCore.hpp"

namespace smesh {

void MeshCore::reset() {
  c1_      = InputGrid{};
  c2_      = InputGrid{};
  a_path_  = InputGrid{};
  b_path_  = AccumGrid{};
  b_valid_ = BoolGrid{};
  d_path_  = InputGrid{};
  d_valid_ = BoolGrid{};
  out_b_   = MeshAccumRow{};
}

// preload mechanics (weights go through D path)
void MeshCore::stepPreloadD(const MeshInputRow& d_shifter_in, bool in_valid) {
  InputGrid next_d = d_path_; // working copy of D path state (updating weights)
  BoolGrid  next_d_valid{};
  // shift new D & valid TB into systolic, but only shift in D if valid is true
  for (std::size_t col = 0; col < kDim; ++col) {
    if (in_valid) {
      next_d[0][col] = d_shifter_in[col];
    }
    next_d_valid[0][col] = in_valid;
  }
  // shift D & valid TB through systolic, but only shift D if corresponding valid is true
  for (std::size_t row = 1; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      if (d_valid_[row - 1][col]) {
        next_d[row][col] = d_path_[row - 1][col];
      }
      next_d_valid[row][col] = d_valid_[row - 1][col];
    }
  }
  // update state
  d_path_  = next_d;
  d_valid_ = next_d_valid;
  // store weights into WS stationary registers if valid is true
  for (std::size_t row = 0; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      if (d_valid_[row][col]) {
        c1_[row][col] = d_path_[row][col];
      }
    }
  }
}

// WS compute mechanics (A goes through A path, B/out_b goes through B path)
void MeshCore::stepWsCompute(const MeshInputRow& a_shifter_in, const MeshAccumRow& b_shifter_in, bool in_valid) {
  InputGrid    next_a{};
  AccumGrid    next_b = b_path_; // working copy of B path state (updatingpsums/results)
  BoolGrid     next_b_valid{};
  MeshAccumRow next_out_b{};

  // what's going into next registers, and what's actually getting latched by them, and what's computed
  for (std::size_t row = 0; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      // what's going into next registers
      const auto a          = col == 0 ? a_shifter_in[row] : a_path_[row][col - 1];  // A moves LR
      const auto b_in       = row == 0 ? b_shifter_in[col] : b_path_[row - 1][col];  // B/out_b moves TB
      const auto b_in_valid = row == 0 ? in_valid          : b_valid_[row - 1][col]; // valid moves TB

      next_a[row][col] = a; // latch a
      if (b_in_valid) { // latch b_in and compute new psum if valid is true
        next_b[row][col] = b_in + static_cast<Acc>(a) * static_cast<Acc>(c1_[row][col]);
      }
      next_b_valid[row][col] = b_in_valid; // latch valid
    }
  }

  // compute out_b emerging from bottom row of B path
  for (std::size_t col = 0; col < kDim; ++col) {
    next_out_b[col] = next_b[kDim - 1][col];
  }
 
  // update state
  a_path_  = next_a;
  b_path_  = next_b;
  b_valid_ = next_b_valid;
  out_b_   = next_out_b;
}

} // namespace smesh
