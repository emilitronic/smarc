// **********************************************************************
// smesh/src/tb_smesh_rs.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jun 27 2026
/*
Focused reservation-station range tests.
*/

#include "SmeshRS.hpp"

#include <cstdint>
#include <cstdio>

namespace {

smesh::SmeshCmd command(smesh::SmeshFunct funct,
                        std::uint64_t rs1,
                        std::uint64_t rs2) {
  return smesh::SmeshCmd{
      u32(static_cast<std::uint32_t>(funct)),
      u64(rs1),
      u64(rs2),
  };
}

bool testLoadRange() {
  smesh::SmeshRS rs;

  const auto config = command(
      smesh::SmeshFunct::Config,
      smesh::packConfig(smesh::ConfigKind::Load, 1, smesh::kDim),
      0);
  if (!rs.allocate(config)) {
    return false;
  }
  if (!rs.complete(rs.entry().rob_id)) {
    return false;
  }

  constexpr std::uint32_t base = 100;
  constexpr smesh::MatrixShape shape{2, 12};
  const auto load = command(
      smesh::SmeshFunct::Mvin2,
      0,
      smesh::packLocal(base, shape));
  if (!rs.allocate(load)) {
    return false;
  }

  const auto& entry = rs.loadEntry();
  return entry.opa.valid && entry.opa_is_dst && !entry.opb.valid &&
         entry.opa.bits.start.raw == base &&
         entry.opa.bits.end.raw == base + 10 &&
         !entry.opa.bits.wraps_around;
}

bool testStoreRange() {
  smesh::SmeshRS rs;

  constexpr std::uint32_t base = 200;
  constexpr smesh::MatrixShape shape{2, 12};
  const auto store = command(
      smesh::SmeshFunct::Mvout,
      0,
      smesh::packLocal(base, shape));
  if (!rs.allocate(store)) {
    return false;
  }

  const auto& entry = rs.storeEntry();
  return entry.opa.valid && !entry.opa_is_dst && !entry.opb.valid &&
         entry.opa.bits.start.raw == base &&
         entry.opa.bits.end.raw == base + 10 &&
         !entry.opa.bits.wraps_around;
}

} // namespace

int main() {
  const bool load_ok = testLoadRange();
  const bool store_ok = testStoreRange();
  std::printf("[SMESH_RS] %s load_range\n", load_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s store_range\n", store_ok ? "PASS" : "FAIL");
  return (load_ok && store_ok) ? 0 : 1;
}
