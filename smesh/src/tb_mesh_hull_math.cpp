// **********************************************************************
// smesh/src/tb_mesh_hull_math.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 03 2026
// Focused MeshHull/MeshCore math smoke test for WS matrix-vector behavior.

#include "MeshHull.hpp"

#include <cstdio>

namespace {

bool rowEquals(const smesh::MeshAccumRow& got, const smesh::MeshAccumRow& expected) {
  for (std::size_t i = 0; i < smesh::kDim; ++i) {
    if (got[i] != expected[i]) {
      return false;
    }
  }
  return true;
}

bool c2ColumnsAre(const smesh::MeshHull& hull, const smesh::MeshInputRow& weights) {
  for (std::size_t row = 0; row < smesh::kDim; ++row) {
    for (std::size_t col = 0; col < smesh::kDim; ++col) {
      if (hull.core().c2()[row][col] != weights[col]) {
        return false;
      }
    }
  }
  return true;
}

void step(smesh::MeshHull& hull, const smesh::MeshHullIn& in, int& cycle) {
  hull.step(in);
  ++cycle;
}

} // namespace

int main() {
  smesh::MeshHull hull;
  hull.reset();

  int cycle = 0;

  const smesh::MeshInputRow weights{1, 2, 3, 4};

  smesh::MeshHullIn set_prop0{};
  set_prop0.req_fire = 1;
  set_prop0.pe_control.propagate = 0;
  step(hull, set_prop0, cycle);

  // Drive a constant D row long enough to fill all rows/cols of the c2 WS buffer.
  for (std::size_t i = 0; i < 3 * smesh::kDim; ++i) {
    smesh::MeshHullIn preload{};
    preload.d_fire = 1;
    preload.d_bits = weights;
    preload.not_paused = 1;
    step(hull, preload, cycle);
  }

  bool ok = c2ColumnsAre(hull, weights);

  smesh::MeshHullIn set_compute_prop{};
  set_compute_prop.req_fire = 1;
  set_compute_prop.pe_control.propagate = 1;
  step(hull, set_compute_prop, cycle);

  const smesh::MeshInputRow a{1, 2, 3, 4};
  smesh::MeshHullIn capture_a{};
  capture_a.a_fire = 1;
  capture_a.a_bits = a;
  step(hull, capture_a, cycle);

  smesh::MeshHullIn use_a{};
  use_a.matmul_id = 7;
  use_a.last_fire = 1;
  use_a.not_paused = 1;
  step(hull, use_a, cycle);

  int valid_cycle = -1;
  smesh::MeshAccumRow result{};
  for (std::size_t i = 0; i < 4 * smesh::kDim; ++i) {
    smesh::MeshHullIn drain{};
    drain.matmul_id = 7;
    drain.last_fire = 1;
    drain.not_paused = 0;
    step(hull, drain, cycle);

    if (hull.out().resp_valid != 0 && hull.out().out_matmul_id == 7) {
      valid_cycle = cycle;
      result = hull.out().resp_data;
      break;
    }
  }

  const smesh::MeshAccumRow expected{10, 20, 30, 40};
  ok = ok && valid_cycle >= 0;
  ok = ok && rowEquals(result, expected);
  ok = ok && hull.out().resp_last != 0;
  ok = ok && hull.out().out_matmul_id == 7;

  std::printf("[MESH_HULL_MATH] %s ws_matvec cycles=%d valid_cycle=%d result={%d,%d,%d,%d}\n",
              ok ? "PASS" : "FAIL",
              cycle,
              valid_cycle,
              static_cast<int>(result[0]),
              static_cast<int>(result[1]),
              static_cast<int>(result[2]),
              static_cast<int>(result[3]));
  return ok ? 0 : 1;
}
