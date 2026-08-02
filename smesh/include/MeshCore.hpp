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

// status/control signals move through systolic along with data
struct MeshCoreCtrl {
  std::uint8_t id = 0;
  bool last  = false;
  bool prop  = false;
  bool valid = false;
};

class MeshCore {
 public:
  using InputGrid = std::array<MeshInputRow, kDim>; // matrix of signals/weights
  using AccumGrid = std::array<MeshAccumRow, kDim>; // matrix of psums/results
  using CtrlRow   = std::array<MeshCoreCtrl, kDim>;
  using CtrlGrid  = std::array<CtrlRow, kDim>;

  void reset();

  // Move one D row top-to-bottom (TB) and capture stationary WS weights
  void stepPreloadD(const MeshInputRow& d_shifter_in, MeshCoreCtrl ctrl);

  // Move A left-to-right (LR) and B/out_b results top-to-bottom (TB) for one WS compute cycle
  void stepWsCompute(const MeshInputRow& a_shifter_in,
                     const MeshAccumRow& b_shifter_in = {},
                     MeshCoreCtrl ctrl = MeshCoreCtrl{0, false, false, true});

  const InputGrid&    c1() const { return c1_; }
  const InputGrid&    c2() const { return c2_; }
  const InputGrid&    aPath() const { return a_path_; }
  const AccumGrid&    bPath() const { return b_path_; }
  const CtrlGrid&     bCtrl() const { return b_ctrl_; }
  const InputGrid&    dPath() const { return d_path_; }
  const CtrlGrid&     dCtrl() const { return d_ctrl_; }
  const MeshAccumRow& outB() const { return out_b_; }
  const CtrlRow&      outBCtrl() const { return out_b_ctrl_; }

 private:
  InputGrid    c1_{};         // WS stationary weight register set 1
  InputGrid    c2_{};         // WS stationary weight register set 2
  InputGrid    a_path_{};     // A vals moving LR
  AccumGrid    b_path_{};     // B/out_b vals moving TB: WS psums/results
  CtrlGrid     b_ctrl_{};     // id/last/prop/valid aligned to b_path_ ()
  InputGrid    d_path_{};     // D/out_c vals moving TB: preload/propagate values
  CtrlGrid     d_ctrl_{};     // id/last/prop/valid aligned to d_path_
  MeshAccumRow out_b_{};      // bottom row emerging from B/out_b path
  CtrlRow      out_b_ctrl_{}; // control emerging with out_b_
};

} // namespace smesh
