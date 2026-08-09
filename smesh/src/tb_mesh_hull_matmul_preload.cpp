// **********************************************************************
// smesh/src/tb_mesh_hull_matmul_preload.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 03 2026
// MeshHull/MeshCore 4x4 WS matmul test with B preloaded through the D path.

#include "MeshHull.hpp"

#include <array>
#include <cstdio>

namespace {

using Matrix = std::array<smesh::MeshInputRow, smesh::kDim>;
using AccMatrix = std::array<smesh::MeshAccumRow, smesh::kDim>;

void step(smesh::MeshHull& hull, const smesh::MeshHullIn& in, int& cycle) {
  hull.step(in);
  ++cycle;
}

smesh::MeshAccumRow multiplyRow(const smesh::MeshInputRow& a, const Matrix& b) {
  smesh::MeshAccumRow out{};
  for (std::size_t col = 0; col < smesh::kDim; ++col) {
    smesh::Acc sum = 0;
    for (std::size_t k = 0; k < smesh::kDim; ++k) {
      sum += static_cast<smesh::Acc>(a[k]) * static_cast<smesh::Acc>(b[k][col]);
    }
    out[col] = sum;
  }
  return out;
}

bool rowEquals(const smesh::MeshAccumRow& got, const smesh::MeshAccumRow& expected) {
  for (std::size_t i = 0; i < smesh::kDim; ++i) {
    if (got[i] != expected[i]) {
      return false;
    }
  }
  return true;
}

bool c2Equals(const smesh::MeshHull& hull, const Matrix& expected) {
  for (std::size_t row = 0; row < smesh::kDim; ++row) {
    for (std::size_t col = 0; col < smesh::kDim; ++col) {
      if (hull.core().c2()[row][col] != expected[row][col]) {
        return false;
      }
    }
  }
  return true;
}

void captureD(smesh::MeshHull& hull, const smesh::MeshInputRow& row, int& cycle) {
  smesh::MeshHullIn capture{};
  capture.d_fire = 1;
  capture.d_bits = row;
  step(hull, capture, cycle);
}

void useDAndMaybeCaptureNext(smesh::MeshHull& hull,
                             const smesh::MeshInputRow* next_row,
                             int& cycle) {
  smesh::MeshHullIn in{};
  in.not_paused = 1;
  if (next_row != nullptr) {
    in.d_fire = 1;
    in.d_bits = *next_row;
  }
  step(hull, in, cycle);
}

void preloadC2ThroughD(smesh::MeshHull& hull, const Matrix& b, int& cycle) {
  smesh::MeshHullIn set_prop0{};
  set_prop0.pe_control.propagate = 0;
  step(hull, set_prop0, cycle);

  const std::array<smesh::MeshInputRow, 4> seq{{
    b[3],
    b[2],
    b[1],
    b[0],
  }};

  captureD(hull, seq[0], cycle);
  for (std::size_t i = 0; i < seq.size(); ++i) {
    const auto* next = i + 1 < seq.size() ? &seq[i + 1] : nullptr;
    useDAndMaybeCaptureNext(hull, next, cycle);
  }

  for (std::size_t i = 0; i < 4 * smesh::kDim; ++i) {
    smesh::MeshHullIn drain{};
    step(hull, drain, cycle);
  }
}

bool runRow(smesh::MeshHull& hull,
            const smesh::MeshInputRow& a,
            std::uint8_t matmul_id,
            int& cycle,
            smesh::MeshAccumRow& result,
            int& valid_cycle) {
  smesh::MeshHullIn capture_a{};
  capture_a.pe_control.propagate = 1;
  capture_a.a_fire = 1;
  capture_a.a_bits = a;
  step(hull, capture_a, cycle);

  smesh::MeshHullIn use_a{};
  use_a.pe_control.propagate = 1;
  use_a.matmul_id = matmul_id;
  use_a.last_fire = 1;
  use_a.not_paused = 1;
  step(hull, use_a, cycle);

  for (std::size_t i = 0; i < 4 * smesh::kDim; ++i) {
    smesh::MeshHullIn drain{};
    drain.pe_control.propagate = 1;
    step(hull, drain, cycle);

    if (hull.out().resp_valid != 0 && hull.out().out_matmul_id == matmul_id) {
      result = hull.out().resp_data;
      valid_cycle = cycle;
      return true;
    }
  }
  return false;
}

} // namespace

int main() {
  const Matrix a{{
    smesh::MeshInputRow{1, 0, 0, 0},
    smesh::MeshInputRow{0, 1, 0, 0},
    smesh::MeshInputRow{1, 2, 3, 4},
    smesh::MeshInputRow{-1, 1, -1, 1},
  }};

  const Matrix b{{
    smesh::MeshInputRow{1, 2, 3, 4},
    smesh::MeshInputRow{5, 6, 7, 8},
    smesh::MeshInputRow{9, 10, 11, 12},
    smesh::MeshInputRow{13, 14, 15, 16},
  }};

  smesh::MeshHull hull;
  hull.reset();

  int cycle = 0;
  preloadC2ThroughD(hull, b, cycle);

  bool ok = c2Equals(hull, b);

  smesh::MeshHullIn set_compute_prop{};
  set_compute_prop.pe_control.propagate = 1;
  step(hull, set_compute_prop, cycle);

  AccMatrix got{};
  AccMatrix expected{};
  std::array<int, smesh::kDim> valid_cycles{};

  for (std::size_t row = 0; row < smesh::kDim; ++row) {
    expected[row] = multiplyRow(a[row], b);
    ok = ok && runRow(hull,
                      a[row],
                      static_cast<std::uint8_t>(row + 1),
                      cycle,
                      got[row],
                      valid_cycles[row]);
    ok = ok && rowEquals(got[row], expected[row]);
  }

  std::printf("[MESH_HULL_MATMUL_PRELOAD] %s cycles=%d rows={%d,%d,%d,%d}\n",
              ok ? "PASS" : "FAIL",
              cycle,
              valid_cycles[0],
              valid_cycles[1],
              valid_cycles[2],
              valid_cycles[3]);
  if (!ok) {
    for (std::size_t row = 0; row < smesh::kDim; ++row) {
      std::printf("  c2[%zu]={%d,%d,%d,%d} expected_b={%d,%d,%d,%d}\n",
                  row,
                  static_cast<int>(hull.core().c2()[row][0]),
                  static_cast<int>(hull.core().c2()[row][1]),
                  static_cast<int>(hull.core().c2()[row][2]),
                  static_cast<int>(hull.core().c2()[row][3]),
                  static_cast<int>(b[row][0]),
                  static_cast<int>(b[row][1]),
                  static_cast<int>(b[row][2]),
                  static_cast<int>(b[row][3]));
      std::printf("  row%zu got={%d,%d,%d,%d} expected={%d,%d,%d,%d}\n",
                  row,
                  static_cast<int>(got[row][0]),
                  static_cast<int>(got[row][1]),
                  static_cast<int>(got[row][2]),
                  static_cast<int>(got[row][3]),
                  static_cast<int>(expected[row][0]),
                  static_cast<int>(expected[row][1]),
                  static_cast<int>(expected[row][2]),
                  static_cast<int>(expected[row][3]));
    }
  }
  return ok ? 0 : 1;
}
