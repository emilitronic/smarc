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

  smesh::MeshInputRow d0{1, 2, 3, 4};
  smesh::MeshInputRow d1{5, 6, 7, 8};
  core.stepPreloadD(d0, smesh::MeshCoreCtrl{1, false, false, true});
  core.stepPreloadD(d1, smesh::MeshCoreCtrl{2, false, true, true});

  bool ok = true;
  ok = ok && core.dPath()[0] == d1;
  ok = ok && core.dPath()[1] == d0;
  ok = ok && core.c1()[0] == d1;
  ok = ok && core.c2()[0] == d0;
  ok = ok && core.c2()[1] == d0;
  ok = ok && core.dCtrl()[0][0].id == 2;
  ok = ok && core.dCtrl()[0][0].prop;
  ok = ok && core.dCtrl()[1][0].id == 1;
  ok = ok && !core.dCtrl()[1][0].prop;

  smesh::MeshInputRow a0{1, 2, 3, 4};
  core.stepWsCompute(a0, smesh::MeshAccumRow{}, smesh::MeshCoreCtrl{3, true, true, true});

  ok = ok && core.aPath()[0][0] == 1;
  ok = ok && core.aPath()[1][0] == 2;
  ok = ok && core.bPath()[0][0] == 1;
  ok = ok && core.bPath()[1][0] == 0;
  ok = ok && core.bCtrl()[0][0].valid;
  ok = ok && core.bCtrl()[0][0].id == 3;
  ok = ok && core.bCtrl()[0][0].last;
  ok = ok && core.bCtrl()[0][0].prop;
  ok = ok && !core.bCtrl()[1][0].valid;
  ok = ok && rowEquals(core.outB(), smesh::MeshAccumRow{0, 0, 0, 0});

  std::printf("[MESH_CORE] %s ws_movement\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
