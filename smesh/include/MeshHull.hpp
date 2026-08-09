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

#include <array>
#include <cstddef>
#include <cstdint>

namespace smesh {
// Hull inputs from outside
struct MeshHullIn {
  // inputs from local memories
  bit          a_fire = 0;
  MeshInputRow a_bits{}; // input type
  bit          b_fire = 0;
  MeshInputRow b_bits{}; // weight type (but widens to handle psums)
  bit          d_fire = 0;
  MeshInputRow d_bits{}; // weight type
  // inputs from transposer
  MeshInputRow transposer_out_col_bits{};
  bit a_is_from_transposer = 0;
  bit b_is_from_transposer = 0;
  bit d_is_from_transposer = 0;
  // inputs from ExCtrl
  ExCtrlMeshPeControl pe_control{};
  std::uint8_t        matmul_id  = 0;
  bit                 last_fire  = 0;
  bit                 not_paused = 0;
};
// Hull outputs to outside
struct MeshHullOut {
  MeshAccumRow resp_data{};
  bit          resp_valid    = 0;
  bit          resp_last     = 0;
  std::uint8_t out_matmul_id = 0;
};

class MeshHull {
 public:
  void reset();
  void step(const MeshHullIn& in);
  void loadC2ForTest(const MeshCore::InputGrid& weights);

  const MeshHullOut& out() const { return out_; }
  const MeshCore& core() const { return core_; }

 private:
  template <typename T>
  using SkewRow = std::array<T, kDim>; // row of kDim valus of type T
  template <typename T>
  using SkewState = std::array<SkewRow<T>, kDim>; // internal delay storage for all lanes

  // Generic skew-register helper
  template <typename T>
  SkewRow<T> stepInputSkew(SkewState<T>& state, const SkewRow<T>& in) {
    SkewRow<T> out{};
    SkewState<T> next = state;

    for (std::size_t lane = 0; lane < kDim; ++lane) {
      out[lane] = lane == 0 ? in[lane] : state[lane][lane - 1]; // lane 0 outputs current input, other lanes use their delayed value
      if (lane > 0) {
        next[lane][0] = in[lane];
        for (std::size_t delay = 1; delay < lane; ++delay) {
          next[lane][delay] = state[lane][delay - 1];
        }
      }
    }
    state = next;
    return out;
  }

  // Reverse skew for output rows: lane 0 is delayed longest, lane kDim-1 is current.
  template <typename T>
  SkewRow<T> stepOutputSkew(SkewState<T>& state, const SkewRow<T>& in) {
    SkewRow<T> out{};
    SkewState<T> next = state;

    for (std::size_t lane = 0; lane < kDim; ++lane) {
      const auto delay_len = kDim - 1 - lane;
      out[lane] = delay_len == 0 ? in[lane] : state[lane][delay_len - 1];
      if (delay_len > 0) {
        next[lane][0] = in[lane];
        for (std::size_t delay = 1; delay < delay_len; ++delay) {
          next[lane][delay] = state[lane][delay - 1];
        }
      }
    }
    state = next;
    return out;
  }

  MeshAccumRow widenInputRow(const MeshInputRow& row) const;

  MeshCore core_{};          // Hull, contains Core systolic array

  MeshInputRow               a_buf_{};  // straight row input 
  MeshAccumRow               b_buf_{};  // straight row partial sums
  MeshInputRow               d_buf_{};  // straight row weights
  SkewState<Elem>            a_skew_{}; // skewed row input
  SkewState<Acc>             b_skew_{}; // skewed row partial sums
  SkewState<Elem>            d_skew_{}; // skewed row weights
  SkewState<MeshCoreControl> control_skew_{}; // propagate control skewed
  SkewState<MeshCoreStatus>  status_skew_{};  // id/last/valid metadata skewed

  SkewState<Acc>            out_b_skew_{};
  SkewState<MeshCoreStatus> out_status_skew_{};
  
  MeshHullOut out_{};
};

} // namespace smesh
