// **********************************************************************
// smesh/tests/reference/loop_matmul/tb_loop_matmul_reference.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 25 2026

#include "LoopMatmulReference.hpp"

#include <cstdio>

namespace {

void show(const char* name, const smesh::tb::PrimitiveCommand& cmd) {
  std::printf("%-8s funct=%2u rs1=%016llx rs2=%016llx\n",
              name, static_cast<unsigned>(cmd.funct),
              static_cast<unsigned long long>(cmd.rs1),
              static_cast<unsigned long long>(cmd.rs2));
}

bool check(const char* name, const smesh::tb::PrimitiveCommand& got,
           smesh::SmeshFunct funct, std::uint64_t rs1, std::uint64_t rs2) {
  if (got.funct == funct && got.rs1 == rs1 && got.rs2 == rs2) {
    return true;
  }
  std::fprintf(stderr,
               "%s: got funct=%u rs1=%016llx rs2=%016llx, expected funct=%u rs1=%016llx rs2=%016llx\n",
               name, static_cast<unsigned>(got.funct),
               static_cast<unsigned long long>(got.rs1),
               static_cast<unsigned long long>(got.rs2),
               static_cast<unsigned>(funct),
               static_cast<unsigned long long>(rs1),
               static_cast<unsigned long long>(rs2));
  return false;
}

} // namespace

int main() {
  static_assert(smesh::kDim == 4, "fixed expectations below describe a 4x4 tile");

  // This is the six-command sequence emitted by gemmini_loop_ws for I=J=K=1.
  const smesh::tb::LoopWsProgram program{{
      {smesh::SmeshFunct::LoopWsBounds, 0, (1ull << 32) | (1ull << 16) | 1ull},
      {smesh::SmeshFunct::LoopWsAddrsAb, 0x1000, 0x2000},
      {smesh::SmeshFunct::LoopWsAddrsDc, 0x3000, 0x4000},
      {smesh::SmeshFunct::LoopWsStridesAb, 64, 96},
      {smesh::SmeshFunct::LoopWsStridesDc, 128, 160},
      {smesh::SmeshFunct::LoopWs, 0, 0},
  }};
  const auto got = smesh::tb::generateSingleTileWs(program);

  show("load A", got.load_a);
  show("load B", got.load_b);
  show("load D", got.load_d);
  show("preload", got.preload);
  show("compute", got.compute);
  show("store C", got.store_c);

  constexpr std::uint64_t shape = (std::uint64_t{4} << 48) | (std::uint64_t{4} << 32);
  constexpr std::uint64_t acc0 = 0x80000000ull;
  bool passed = true;
  passed &= check("load A", got.load_a, smesh::SmeshFunct::Mvin, 0x1000, shape);
  passed &= check("load B", got.load_b, smesh::SmeshFunct::Mvin2, 0x2000, shape | 4);
  passed &= check("load D", got.load_d, smesh::SmeshFunct::Mvin3, 0x3000, shape | acc0);
  passed &= check("preload", got.preload, smesh::SmeshFunct::Preload,
                  shape | 4, shape | acc0);
  const auto compute_addend = smesh::unpackLocal(got.compute.rs2);
  const bool compute_ok = got.compute.funct == smesh::SmeshFunct::ComputeFlip &&
                          got.compute.rs1 == shape &&
                          compute_addend.shape.rows == 4 && compute_addend.shape.cols == 4 &&
                          smesh::makeLocalAddr(compute_addend.row).is_garbage();
  if (!compute_ok) {
    std::fprintf(stderr, "compute: expected FLIP with A at SPAD row 0 and a garbage addend\n");
  }
  passed &= compute_ok;
  passed &= check("store C", got.store_c, smesh::SmeshFunct::Mvout,
                  0x4000, shape | acc0);

  std::puts(passed ? "[LOOP_MATMUL_REFERENCE] PASS one_tile_ws"
                   : "[LOOP_MATMUL_REFERENCE] FAIL one_tile_ws");
  return passed ? 0 : 1;
}
