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

smesh::MeshCoreControlRow controlRow(bool prop) {
  smesh::MeshCoreControlRow row{};
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    row[lane].prop = prop;
  }
  return row;
}

smesh::MeshCoreStatusRow statusRow(const smesh::MeshCoreStatus& status) {
  smesh::MeshCoreStatusRow row{};
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    row[lane] = status;
  }
  return row;
}

} // namespace

int main() {
  smesh::MeshCore core;
  core.reset();

  smesh::MeshInputRow d0{1, 2, 3, 4};
  smesh::MeshInputRow d1{5, 6, 7, 8};
  smesh::MeshCoreIn preload0{};
  preload0.in_d = d0;
  preload0.control = controlRow(false);
  preload0.status = statusRow(smesh::MeshCoreStatus{1, false, true});
  core.step(preload0);

  smesh::MeshCoreIn preload1{};
  preload1.in_d = d1;
  preload1.control = controlRow(true);
  preload1.status = statusRow(smesh::MeshCoreStatus{2, false, true});
  core.step(preload1);

  bool ok = true;
  ok = ok && core.dPath()[0] == d1;
  ok = ok && core.dPath()[1] == d0;
  ok = ok && core.c1()[0] == d1;
  ok = ok && core.c2()[0] == d0;
  ok = ok && core.c2()[1] == d0;
  ok = ok && core.controlPath()[0][0].prop;
  ok = ok && core.statusPath()[0][0].in_id == 2;
  ok = ok && core.statusPath()[1][0].in_id == 1;
  ok = ok && !core.controlPath()[1][0].prop;

  smesh::MeshInputRow a0{1, 2, 3, 4};
  smesh::MeshInputRow d2{9, 10, 11, 12};
  smesh::MeshCoreIn compute0{};
  compute0.in_a = a0;
  compute0.in_d = d2;
  compute0.control = controlRow(true);
  compute0.status = statusRow(smesh::MeshCoreStatus{3, true, true});
  core.step(compute0);

  ok = ok && core.c1()[0] == d2;
  ok = ok && core.aPath()[0][0] == 1;
  ok = ok && core.aPath()[1][0] == 2;
  ok = ok && core.bPath()[0][0] == 1;
  ok = ok && core.bPath()[1][0] == 2;
  ok = ok && core.controlPath()[0][0].prop;
  ok = ok && core.statusPath()[0][0].valid;
  ok = ok && core.statusPath()[0][0].in_id == 3;
  ok = ok && core.statusPath()[0][0].in_last;
  ok = ok && core.statusPath()[1][0].valid;
  ok = ok && core.statusPath()[1][0].in_id == 2;
  ok = ok && rowEquals(core.outB(), smesh::MeshAccumRow{0, 0, 0, 0});

  std::printf("[MESH_CORE] %s ws_movement\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
