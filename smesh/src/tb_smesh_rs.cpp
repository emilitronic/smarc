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

bool testStoreSpadRange() {
  smesh::SmeshRS rs;

  constexpr std::uint32_t destination_base = 1000;
  constexpr std::uint32_t source_base = 200;
  constexpr smesh::MatrixShape shape{2, smesh::kDim};
  const auto store_spad = command(
      smesh::SmeshFunct::StoreSpad,
      smesh::packStoreSpadDestination(destination_base, 1),
      smesh::packLocal(source_base, shape));
  if (!rs.allocate(store_spad)) {
    return false;
  }

  const auto& entry = rs.storeEntry();
  return entry.opa.valid && entry.opa_is_dst && entry.opb.valid &&
         entry.opa.bits.start.raw == destination_base &&
         entry.opa.bits.end.raw == destination_base + shape.rows &&
         !entry.opa.bits.wraps_around &&
         entry.opb.bits.start.raw == source_base &&
         entry.opb.bits.end.raw == source_base + shape.rows &&
         !entry.opb.bits.wraps_around;
}

bool testPreloadRange() {
  smesh::SmeshRS rs;

  const auto config = command(
      smesh::SmeshFunct::Config,
      smesh::packConfigExecuteRs1(1),
      smesh::packConfigExecuteRs2(2));
  if (!rs.allocate(config)) {
    return false;
  }
  if (!rs.complete(rs.entry().rob_id)) {
    return false;
  }

  constexpr std::uint32_t source_base = 100;
  constexpr std::uint32_t destination_base = 400;
  constexpr smesh::MatrixShape source_shape{2, smesh::kDim};
  constexpr smesh::MatrixShape destination_shape{3, smesh::kDim};
  const auto preload = command(
      smesh::SmeshFunct::Preload,
      smesh::packLocal(source_base, source_shape),
      smesh::packLocal(destination_base, destination_shape));
  if (!rs.allocate(preload)) {
    return false;
  }

  const auto& entry = rs.executeEntry();
  return entry.opa.valid && entry.opa_is_dst && entry.opb.valid &&
         entry.opa.bits.start.raw == destination_base &&
         entry.opa.bits.end.raw == destination_base + 6 &&
         !entry.opa.bits.wraps_around &&
         entry.opb.bits.start.raw == source_base &&
         entry.opb.bits.end.raw == source_base + source_shape.rows &&
         !entry.opb.bits.wraps_around;
}

} // namespace

int main() {
  const bool load_ok = testLoadRange();
  const bool store_ok = testStoreRange();
  const bool store_spad_ok = testStoreSpadRange();
  const bool preload_ok = testPreloadRange();
  std::printf("[SMESH_RS] %s load_range\n", load_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s store_range\n", store_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s store_spad_range\n",
              store_spad_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s preload_range\n",
              preload_ok ? "PASS" : "FAIL");
  return (load_ok && store_ok && store_spad_ok && preload_ok) ? 0 : 1;
}
