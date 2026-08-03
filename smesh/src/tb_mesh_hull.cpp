// **********************************************************************
// smesh/src/tb_mesh_hull.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 02 2026
// Focused plain-C++ MeshHull boundary-to-core test.

#include "MeshHull.hpp"

#include <cstdio>

int main() {
  smesh::MeshHull hull;
  hull.reset();

  smesh::MeshHullIn preload0{};
  preload0.d_fire = 1;
  preload0.d_bits = smesh::ExCtrlMeshInput{0x04030201u, 1};
  preload0.pe_control.propagate = 0;
  preload0.matmul_id = 1;
  hull.step(preload0);

  smesh::MeshHullIn preload1{};
  preload1.d_fire = 1;
  preload1.d_bits = smesh::ExCtrlMeshInput{0x08070605u, 1};
  preload1.pe_control.propagate = 1;
  preload1.matmul_id = 2;
  hull.step(preload1);

  smesh::MeshHullIn compute0{};
  compute0.a_fire = 1;
  compute0.a_bits = smesh::ExCtrlMeshInput{0x04030201u, 1};
  compute0.d_fire = 1;
  compute0.d_bits = smesh::ExCtrlMeshInput{0x0c0b0a09u, 1};
  compute0.pe_control.propagate = 1;
  compute0.matmul_id = 3;
  compute0.last_fire = 1;
  hull.step(compute0);

  bool ok = true;
  ok = ok && hull.core().c1()[0][0] == 9;
  ok = ok && hull.core().c2()[0][0] == 1;
  ok = ok && hull.core().bPath()[0][0] == 1;
  ok = ok && hull.core().bPath()[1][0] == 2;
  ok = ok && hull.core().controlPath()[0][0].in_id == 3;
  ok = ok && hull.core().controlPath()[0][0].in_last;

  std::printf("[MESH_HULL] %s boundary_to_core\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
