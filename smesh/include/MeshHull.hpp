// **********************************************************************
// smesh/include/MeshHull.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 2 2026
/*
Timing wrapper around MeshCore.

This is not a Cascade component. A later Mesher component can own this helper
and connect it to valid/ready ports. MeshHull is where input/output skew and
mesh-boundary timing belongs.
*/

#pragma once

#include "ExCtrlMeshCntlQueue.hpp"
#include "ExCtrlMeshInSelPad.hpp"
#include "MeshCore.hpp"

#include <cstdint>

namespace smesh {

struct MeshHullIn {
  bit a_fire = 0;
  MeshInputRow a_bits{};
  bit b_fire = 0;
  MeshInputRow b_bits{};
  bit d_fire = 0;
  MeshInputRow d_bits{};

  u64 transposer_out_col_bits = 0;
  bit a_is_from_transposer    = 0;
  bit b_is_from_transposer    = 0;
  bit d_is_from_transposer    = 0;

  ExCtrlMeshPeControl pe_control{};
  bit req_fire = 0;
  std::uint8_t matmul_id = 0;
  bit last_fire = 0;
  bit pause = 0;
};

struct MeshHullOut {
  MeshAccumRow resp_data{};
  bit resp_valid = 0;
  bit resp_last = 0;
  std::uint8_t out_matmul_id = 0;
};

class MeshHull {
 public:
  void reset();
  void step(const MeshHullIn& in);

  const MeshHullOut& out() const { return out_; }
  const MeshCore& core() const { return core_; }

 private:
  MeshInputRow inputRowFromBits(u64 data) const;
  MeshAccumRow widenInputRow(const MeshInputRow& row) const;

  MeshCore core_{};
  MeshInputRow a_buf_{}; // input
  MeshAccumRow b_buf_{}; // partial sums
  MeshInputRow d_buf_{}; // weights
  MeshHullOut out_{};
};

} // namespace smesh
