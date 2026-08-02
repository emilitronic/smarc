// **********************************************************************
// smesh/src/tb_mesh_core.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 01 2026
// Focused plain-C++ MeshCore movement test.

#include "MeshCore.hpp"

#include <cstdio>

namespace {

bool rowEquals(const smesh::MeshAccumRow& row, const smesh::MeshAccumRow& expected) {
  for (std::size_t col = 0; col < smesh::kDim; ++col) {
    if (row[col] != expected[col]) {
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  smesh::MeshCore core;
  core.reset();

  smesh::MeshInputRow b0{1, 2, 3, 4};
  smesh::MeshInputRow b1{5, 6, 7, 8};
  core.stepPreloadB(b0);
  core.stepPreloadB(b1);

  bool ok = true;
  ok = ok && core.weights()[0] == b1;
  ok = ok && core.weights()[1] == b0;

  smesh::MeshInputRow a0{1, 2, 3, 4};
  core.stepWsCompute(a0);

  ok = ok && core.aPipe()[0][0] == 1;
  ok = ok && core.aPipe()[1][0] == 2;
  ok = ok && core.psumPipe()[0][0] == 5;
  ok = ok && core.psumPipe()[1][0] == 2;
  ok = ok && rowEquals(core.bottomPsum(), smesh::MeshAccumRow{0, 0, 0, 0});

  std::printf("[MESH_CORE] %s ws_movement\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
