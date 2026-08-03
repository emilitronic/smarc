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
#include <cstdint>

namespace smesh {

using MeshInputRow = std::array<Elem, kDim>;
using MeshAccumRow = std::array<Acc, kDim>;

// control signals entering the systolic core
struct MeshCoreControl {
  std::uint8_t in_id = 0;
  bool in_last = false;
  bool prop    = false;
  bool valid   = false;
};

using MeshCoreControlRow = std::array<MeshCoreControl, kDim>;

// one cycle of boundary inputs into the systolic core
struct MeshCoreIn {
  MeshInputRow in_a{};
  MeshAccumRow in_b{};
  MeshInputRow in_d{};
  MeshCoreControlRow control{};
};

class MeshCore {
 public:
  using InputGrid = std::array<MeshInputRow, kDim>; // matrix of signals/weights
  using AccumGrid = std::array<MeshAccumRow, kDim>; // matrix of psums/results
  using CtrlRow   = MeshCoreControlRow;
  using CtrlGrid  = std::array<CtrlRow, kDim>;

  void reset();

  // Advance all LR/TB systolic paths by one cycle.
  void step(const MeshCoreIn& in);

  const InputGrid&    c1() const { return c1_; }
  const InputGrid&    c2() const { return c2_; }
  const InputGrid&    aPath() const { return a_path_; }
  const AccumGrid&    bPath() const { return b_path_; }
  const InputGrid&    dPath() const { return d_path_; }
  const CtrlGrid&     controlPath() const { return control_path_; }
  const MeshAccumRow& outB() const { return out_b_; }
  const CtrlRow&      outBControl() const { return out_b_control_; }

 private:
  InputGrid    c1_{};            // WS stationary weight register set 1
  InputGrid    c2_{};            // WS stationary weight register set 2
  InputGrid    a_path_{};        // A vals moving LR
  AccumGrid    b_path_{};        // B/out_b vals moving TB: WS psums/results
  InputGrid    d_path_{};        // D/out_c vals moving TB: preload/propagate values
  CtrlGrid     control_path_{};  // in_id/in_last/prop/valid flowing TB
  MeshAccumRow out_b_{};         // bottom row emerging from B/out_b path
  CtrlRow      out_b_control_{}; // control emerging with out_b_
};

} // namespace smesh
