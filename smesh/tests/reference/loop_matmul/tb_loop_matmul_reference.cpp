// **********************************************************************
// smesh/tests/reference/loop_matmul/tb_loop_matmul_reference.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 25 2026
/*
cmake --build build --target tb_loop_matmul_reference -j >/dev/null 2>&1
./build/smesh/tb_loop_matmul_reference
 ctest --test-dir build -R '^smesh_reference_loop_matmul_small_ws$' --output-on-failure
*/
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
  passed &= checkCount("one tile execute stream", got.execute.size(), 2);
  passed &= checkCount("one tile store", got.store_c.size(), 1);
  if (!passed) return 1;

  std::puts("LOOP_WS case: I=1 J=1 K=1");
  show("load A", got.load_a[0]);
  show("load B", got.load_b[0]);
  show("load D", got.load_d[0]);
  show("exec[0] PRE", got.execute[0]);
  show("exec[1] CMP", got.execute[1]);
  show("store C", got.store_c[0]);

  constexpr std::uint64_t shape = (std::uint64_t{4} << 48) | (std::uint64_t{4} << 32);
  constexpr std::uint64_t acc0 = 0x80000000ull;
  passed &= check("load A", got.load_a[0], smesh::SmeshFunct::Mvin, 0x1000, shape);
  passed &= check("load B", got.load_b[0], smesh::SmeshFunct::Mvin2, 0x2000, shape | 4);
  passed &= check("load D", got.load_d[0], smesh::SmeshFunct::Mvin3, 0x3000, shape | acc0);
  passed &= check("preload", got.execute[0], smesh::SmeshFunct::Preload, shape | 4, shape | acc0);
  const bool compute_ok = got.execute[1].funct == smesh::SmeshFunct::ComputeFlip &&
                          got.execute[1].rs1 == shape && isGarbageTile(got.execute[1].rs2);
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
  passed &= checkCount("two tile execute stream", two.execute.size(), 4);
  passed &= checkCount("two tile store", two.store_c.size(), 2);
  if (!passed) return 1;

  std::puts("LOOP_WS case: I=2 J=1 K=1");
  show("load A[0]", two.load_a[0]);
  show("load A[1]", two.load_a[1]);
  show("load B", two.load_b[0]);
  show("load D[0]", two.load_d[0]);
  show("load D[1]", two.load_d[1]);
  show("exec[0] PRE", two.execute[0]);
  show("exec[1] CMP", two.execute[1]);
  show("exec[2] PRE", two.execute[2]);
  show("exec[3] CMP", two.execute[3]);
  show("store C[0]", two.store_c[0]);
  show("store C[1]", two.store_c[1]);

  passed &= check("A tile 0", two.load_a[0], smesh::SmeshFunct::Mvin, 0x1000, shape);
  passed &= check("A tile 1", two.load_a[1], smesh::SmeshFunct::Mvin, 0x1100, shape | 4);
  passed &= check("B tile", two.load_b[0], smesh::SmeshFunct::Mvin2, 0x2000, shape | 12);
  passed &= check("D tile 0", two.load_d[0], smesh::SmeshFunct::Mvin3, 0x3000, shape | acc0);
  passed &= check("D tile 1", two.load_d[1], smesh::SmeshFunct::Mvin3, 0x3800, shape | acc0 | 4);
  passed &= check("preload 0", two.execute[0], smesh::SmeshFunct::Preload,
                  shape | 12, shape | acc0);
  const bool preload_reuses_b = two.execute[2].funct == smesh::SmeshFunct::Preload &&
                                isGarbageTile(two.execute[2].rs1) &&
                                two.execute[2].rs2 == (shape | acc0 | 4);
  if (!preload_reuses_b) {
    std::fprintf(stderr, "preload 1: expected garbage B and accumulator row 4\n");
  }
  passed &= preload_reuses_b;
  const bool computes_ok = two.execute[1].funct == smesh::SmeshFunct::ComputeFlip &&
                           two.execute[1].rs1 == shape && isGarbageTile(two.execute[1].rs2) &&
                           two.execute[3].funct == smesh::SmeshFunct::ComputeStay &&
                           two.execute[3].rs1 == (shape | 4) && isGarbageTile(two.execute[3].rs2);
  if (!computes_ok) {
    std::fprintf(stderr, "compute: expected FLIP for A row 0, then STAY for A row 4\n");
  }
  passed &= computes_ok;
  passed &= check("store C tile 0", two.store_c[0], smesh::SmeshFunct::Mvout,
                  0x4000, shape | acc0);
  passed &= check("store C tile 1", two.store_c[1], smesh::SmeshFunct::Mvout,
                  0x4280, shape | acc0 | 4);

  // Advance J once. The DMA generators combine the two column tiles, while
  // execute still emits one PRELOAD/COMPUTE pair per tile.
  auto two_j_program = program;
  two_j_program[0].rs2 = (1ull << 32) | (2ull << 16) | 1ull;
  two_j_program[5].rs1 = 2ull << 16;
  const auto two_j = smesh::tb::generateWsCommands(two_j_program);
  passed &= checkCount("two J load A", two_j.load_a.size(), 1);
  passed &= checkCount("two J load B", two_j.load_b.size(), 1);
  passed &= checkCount("two J load D", two_j.load_d.size(), 1);
  passed &= checkCount("two J execute stream", two_j.execute.size(), 4);
  passed &= checkCount("two J store", two_j.store_c.size(), 1);
  if (!passed) return 1;

  std::puts("LOOP_WS case: I=1 J=2 K=1");
  show("load A", two_j.load_a[0]);
  show("load B", two_j.load_b[0]);
  show("load D", two_j.load_d[0]);
  show("exec[0] PRE", two_j.execute[0]);
  show("exec[1] CMP", two_j.execute[1]);
  show("exec[2] PRE", two_j.execute[2]);
  show("exec[3] CMP", two_j.execute[3]);
  show("store C", two_j.store_c[0]);

  constexpr std::uint64_t wide_shape = (std::uint64_t{4} << 48) | (std::uint64_t{8} << 32);
  passed &= check("J load A", two_j.load_a[0], smesh::SmeshFunct::Mvin, 0x1000, shape);
  passed &= check("J load B", two_j.load_b[0], smesh::SmeshFunct::Mvin2,
                  0x2000, wide_shape | 8);
  passed &= check("J load D", two_j.load_d[0], smesh::SmeshFunct::Mvin3,
                  0x3000, wide_shape | acc0);
  passed &= check("J preload 0", two_j.execute[0], smesh::SmeshFunct::Preload,
                  shape | 8, shape | acc0);
  passed &= check("J preload 1", two_j.execute[2], smesh::SmeshFunct::Preload,
                  shape | 12, shape | acc0 | 4);
  const bool j_computes_ok = two_j.execute[1].funct == smesh::SmeshFunct::ComputeFlip &&
                             two_j.execute[1].rs1 == shape &&
                             isGarbageTile(two_j.execute[1].rs2) &&
                             two_j.execute[3].funct == smesh::SmeshFunct::ComputeFlip &&
                             two_j.execute[3].rs1 == shape &&
                             isGarbageTile(two_j.execute[3].rs2);
  if (!j_computes_ok) {
    std::fprintf(stderr, "J computes: expected two FLIPs using A row 0\n");
  }
  passed &= j_computes_ok;
  passed &= check("J store C", two_j.store_c[0], smesh::SmeshFunct::Mvout,
                  0x4000, wide_shape | acc0);

  // Advance K once. A's K tiles combine into one wider load; B loads once per K.
  auto two_k_program = program;
  two_k_program[0].rs2 = (2ull << 32) | (1ull << 16) | 1ull;
  two_k_program[5].rs1 = 2ull << 16;
  const auto two_k = smesh::tb::generateWsCommands(two_k_program);
  passed &= checkCount("two K load A", two_k.load_a.size(), 1);
  passed &= checkCount("two K load B", two_k.load_b.size(), 2);
  passed &= checkCount("two K load D", two_k.load_d.size(), 1);
  passed &= checkCount("two K execute stream", two_k.execute.size(), 4);
  passed &= checkCount("two K store C", two_k.store_c.size(), 1);
  if (!passed) return 1;

  std::puts("LOOP_WS case: I=1 J=1 K=2");
  show("load A", two_k.load_a[0]);
  show("load B[0]", two_k.load_b[0]);
  show("load B[1]", two_k.load_b[1]);
  show("load D", two_k.load_d[0]);
  show("exec[0] PRE", two_k.execute[0]);
  show("exec[1] CMP", two_k.execute[1]);
  show("exec[2] PRE", two_k.execute[2]);
  show("exec[3] CMP", two_k.execute[3]);
  show("store C", two_k.store_c[0]);

  passed &= check("K load A", two_k.load_a[0], smesh::SmeshFunct::Mvin,
                  0x1000, wide_shape);
  passed &= check("K load B 0", two_k.load_b[0], smesh::SmeshFunct::Mvin2,
                  0x2000, shape | 8);
  passed &= check("K load B 1", two_k.load_b[1], smesh::SmeshFunct::Mvin2,
                  0x2180, shape | 12);
  passed &= check("K load D", two_k.load_d[0], smesh::SmeshFunct::Mvin3,
                  0x3000, shape | acc0);
  passed &= check("K preload 0", two_k.execute[0], smesh::SmeshFunct::Preload,
                  shape | 8, shape | acc0);
  passed &= check("K preload 1", two_k.execute[2], smesh::SmeshFunct::Preload,
                  shape | 12, shape | acc0 | smesh::kLocalAddrAccumulateMask);
  const bool k_computes_ok =
      two_k.execute[1].funct == smesh::SmeshFunct::ComputeFlip &&
      two_k.execute[1].rs1 == shape && isGarbageTile(two_k.execute[1].rs2) &&
      two_k.execute[3].funct == smesh::SmeshFunct::ComputeFlip &&
      two_k.execute[3].rs1 == (shape | 4) && isGarbageTile(two_k.execute[3].rs2);
  if (!k_computes_ok) {
    std::fprintf(stderr, "K computes: expected FLIP for A K rows 0 and 4\n");
  }
  passed &= k_computes_ok;
  passed &= check("K store C", two_k.store_c[0], smesh::SmeshFunct::Mvout,
                  0x4000, shape | acc0);

  std::puts(passed ? "[LOOP_MATMUL_REFERENCE] PASS I=1,J=1,K=1 I=2,J=1,K=1 I=1,J=2,K=1 I=1,J=1,K=2"
                   : "[LOOP_MATMUL_REFERENCE] FAIL I=1,J=1,K=1 I=2,J=1,K=1 I=1,J=2,K=1 I=1,J=1,K=2");
  return passed ? 0 : 1;
}
