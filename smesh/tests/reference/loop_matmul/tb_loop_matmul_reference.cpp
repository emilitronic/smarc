// **********************************************************************
// smesh/tests/reference/loop_matmul/tb_loop_matmul_reference.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 25 2026

#include "LoopMatmulReference.hpp"

#include <cstdio>

namespace {

void show(const char* name, const smesh::tb::PrimitiveCommand& cmd) {
  std::printf("%-10s funct=%2u rs1=%016llx rs2=%016llx\n",
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

bool checkCount(const char* name, std::size_t actual, std::size_t expected) {
  if (actual == expected) {
    return true;
  }
  std::fprintf(stderr, "%s: got %zu commands, expected %zu\n", name, actual, expected);
  return false;
}

bool isGarbageTile(std::uint64_t packed) {
  const auto matrix = smesh::unpackLocal(packed);
  return matrix.shape.rows == 4 && matrix.shape.cols == 4 &&
         smesh::makeLocalAddr(matrix.row).is_garbage();
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
  const auto got = smesh::tb::generateWsCommands(program);

  bool passed = true;
  passed &= checkCount("one tile load A", got.load_a.size(), 1);
  passed &= checkCount("one tile load B", got.load_b.size(), 1);
  passed &= checkCount("one tile load D", got.load_d.size(), 1);
  passed &= checkCount("one tile preload", got.preload.size(), 1);
  passed &= checkCount("one tile compute", got.compute.size(), 1);
  passed &= checkCount("one tile store", got.store_c.size(), 1);
  if (!passed) return 1;

  std::puts("one_tile_ws:");
  show("load A", got.load_a[0]);
  show("load B", got.load_b[0]);
  show("load D", got.load_d[0]);
  show("preload", got.preload[0]);
  show("compute", got.compute[0]);
  show("store C", got.store_c[0]);

  constexpr std::uint64_t shape = (std::uint64_t{4} << 48) | (std::uint64_t{4} << 32);
  constexpr std::uint64_t acc0 = 0x80000000ull;
  passed &= check("load A", got.load_a[0], smesh::SmeshFunct::Mvin, 0x1000, shape);
  passed &= check("load B", got.load_b[0], smesh::SmeshFunct::Mvin2, 0x2000, shape | 4);
  passed &= check("load D", got.load_d[0], smesh::SmeshFunct::Mvin3, 0x3000, shape | acc0);
  passed &= check("preload", got.preload[0], smesh::SmeshFunct::Preload,
                  shape | 4, shape | acc0);
  const bool compute_ok = got.compute[0].funct == smesh::SmeshFunct::ComputeFlip &&
                          got.compute[0].rs1 == shape && isGarbageTile(got.compute[0].rs2);
  if (!compute_ok) {
    std::fprintf(stderr, "compute: expected FLIP with A at SPAD row 0 and a garbage addend\n");
  }
  passed &= compute_ok;
  passed &= check("store C", got.store_c[0], smesh::SmeshFunct::Mvout,
                  0x4000, shape | acc0);

  // Advance I once. B moves to a separate SPAD region so A's second tile cannot overlap it.
  auto two_tile_program = program;
  two_tile_program[0].rs2 = (1ull << 32) | (1ull << 16) | 2ull;
  two_tile_program[5].rs1 = 2ull << 16;
  const auto two = smesh::tb::generateWsCommands(two_tile_program);
  passed &= checkCount("two tile load A", two.load_a.size(), 2);
  passed &= checkCount("two tile load B", two.load_b.size(), 1);
  passed &= checkCount("two tile load D", two.load_d.size(), 2);
  passed &= checkCount("two tile preload", two.preload.size(), 2);
  passed &= checkCount("two tile compute", two.compute.size(), 2);
  passed &= checkCount("two tile store", two.store_c.size(), 2);
  if (!passed) return 1;

  std::puts("two_tile_i_ws:");
  show("load A[0]", two.load_a[0]);
  show("load A[1]", two.load_a[1]);
  show("load B", two.load_b[0]);
  show("load D[0]", two.load_d[0]);
  show("load D[1]", two.load_d[1]);
  show("preload[0]", two.preload[0]);
  show("preload[1]", two.preload[1]);
  show("compute[0]", two.compute[0]);
  show("compute[1]", two.compute[1]);
  show("store C[0]", two.store_c[0]);
  show("store C[1]", two.store_c[1]);

  passed &= check("A tile 0", two.load_a[0], smesh::SmeshFunct::Mvin, 0x1000, shape);
  passed &= check("A tile 1", two.load_a[1], smesh::SmeshFunct::Mvin, 0x1100, shape | 4);
  passed &= check("B tile", two.load_b[0], smesh::SmeshFunct::Mvin2, 0x2000, shape | 12);
  passed &= check("D tile 0", two.load_d[0], smesh::SmeshFunct::Mvin3, 0x3000, shape | acc0);
  passed &= check("D tile 1", two.load_d[1], smesh::SmeshFunct::Mvin3, 0x3800, shape | acc0 | 4);
  passed &= check("preload 0", two.preload[0], smesh::SmeshFunct::Preload,
                  shape | 12, shape | acc0);
  const bool preload_reuses_b = two.preload[1].funct == smesh::SmeshFunct::Preload &&
                                isGarbageTile(two.preload[1].rs1) &&
                                two.preload[1].rs2 == (shape | acc0 | 4);
  if (!preload_reuses_b) {
    std::fprintf(stderr, "preload 1: expected garbage B and accumulator row 4\n");
  }
  passed &= preload_reuses_b;
  const bool computes_ok = two.compute[0].funct == smesh::SmeshFunct::ComputeFlip &&
                           two.compute[0].rs1 == shape && isGarbageTile(two.compute[0].rs2) &&
                           two.compute[1].funct == smesh::SmeshFunct::ComputeStay &&
                           two.compute[1].rs1 == (shape | 4) && isGarbageTile(two.compute[1].rs2);
  if (!computes_ok) {
    std::fprintf(stderr, "compute: expected FLIP for A row 0, then STAY for A row 4\n");
  }
  passed &= computes_ok;
  passed &= check("store C tile 0", two.store_c[0], smesh::SmeshFunct::Mvout,
                  0x4000, shape | acc0);
  passed &= check("store C tile 1", two.store_c[1], smesh::SmeshFunct::Mvout,
                  0x4280, shape | acc0 | 4);

  std::puts(passed ? "[LOOP_MATMUL_REFERENCE] PASS one_tile_ws two_tile_i_ws"
                   : "[LOOP_MATMUL_REFERENCE] FAIL one_tile_ws two_tile_i_ws");
  return passed ? 0 : 1;
}
