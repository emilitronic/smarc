// **********************************************************************
// smesh/src/ExCtrlMeshInSelPad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026

#include "ExCtrlMeshInSelPad.hpp"

namespace smesh {

ExCtrlMeshInSelPad::ExCtrlMeshInSelPad(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(a_bank,
             b_bank,
             d_bank,
             a_bank_acc,
             b_bank_acc,
             d_bank_acc,
             a_read_from_acc,
             b_read_from_acc)
      .reads(d_read_from_acc,
             a_unpadded_cols,
             b_unpadded_cols,
             d_unpadded_cols,
             a_fire,
             b_fire,
             d_fire,
             spad_read_data)
      .reads(accum_read_data)
      .writes(mesh_a, mesh_b, mesh_d);
}

void ExCtrlMeshInSelPad::update() {
  // TODO: select/pad spad or accumulator read data for mesh A/B/D inputs.
  mesh_a = ExCtrlMeshInput{};
  mesh_b = ExCtrlMeshInput{};
  mesh_d = ExCtrlMeshInput{};
}

} // namespace smesh
