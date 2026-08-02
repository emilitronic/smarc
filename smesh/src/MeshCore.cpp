// **********************************************************************
// smesh/src/MeshCore.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 1 2026

#include "MeshCore.hpp"

namespace smesh {

void MeshCore::reset() {
  b_pipe_ = InputGrid{};
  weights_ = InputGrid{};
  a_pipe_ = InputGrid{};
  psum_pipe_ = AccumGrid{};
  bottom_psum_ = MeshAccumRow{};
}

void MeshCore::stepPreloadB(const MeshInputRow& b_top) {
  InputGrid next_b{};

  next_b[0] = b_top;
  for (std::size_t row = 1; row < kDim; ++row) {
    next_b[row] = b_pipe_[row - 1];
  }

  b_pipe_ = next_b;
  weights_ = b_pipe_;
}

void MeshCore::stepWsCompute(const MeshInputRow& a_left, const MeshAccumRow& psum_top) {
  InputGrid next_a{};
  AccumGrid next_psum{};
  MeshAccumRow next_bottom{};

  for (std::size_t row = 0; row < kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      const auto a = col == 0 ? a_left[row] : a_pipe_[row][col - 1];
      const auto psum = row == 0 ? psum_top[col] : psum_pipe_[row - 1][col];

      next_a[row][col] = a;
      next_psum[row][col] = psum + static_cast<Acc>(a) * static_cast<Acc>(weights_[row][col]);
    }
  }

  for (std::size_t col = 0; col < kDim; ++col) {
    next_bottom[col] = next_psum[kDim - 1][col];
  }

  a_pipe_ = next_a;
  psum_pipe_ = next_psum;
  bottom_psum_ = next_bottom;
}

} // namespace smesh
