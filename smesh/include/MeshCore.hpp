// **********************************************************************
// smesh/include/MeshCore.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 1 2026
/*
Plain C++ core systolic-array state model.

This is not a Cascade component. Mesher will own this helper and call step
methods once per simulated cycle.
*/

#pragma once

#include "SmeshTypes.hpp"

#include <array>
#include <cstddef>

namespace smesh {

using MeshInputRow = std::array<Elem, kDim>;
using MeshAccumRow = std::array<Acc, kDim>;

class MeshCore {
 public:
  using InputGrid = std::array<MeshInputRow, kDim>;
  using AccumGrid = std::array<MeshAccumRow, kDim>;

  void reset();

  // Move one B row top-to-bottom and capture the row currently at each PE row.
  void stepPreloadB(const MeshInputRow& b_top);

  // Move A left-to-right and partial sums top-to-bottom for one WS compute cycle.
  void stepWsCompute(const MeshInputRow& a_left, const MeshAccumRow& psum_top = {});

  const InputGrid& weights() const { return weights_; }
  const InputGrid& aPipe() const { return a_pipe_; }
  const AccumGrid& psumPipe() const { return psum_pipe_; }
  const MeshAccumRow& bottomPsum() const { return bottom_psum_; }

 private:
  InputGrid b_pipe_{};
  InputGrid weights_{};
  InputGrid a_pipe_{};
  AccumGrid psum_pipe_{};
  MeshAccumRow bottom_psum_{};
};

} // namespace smesh
