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

bool testLocalAddr() {
  constexpr auto sp = smesh::makeSpAddr(6);
  constexpr auto acc = smesh::makeAccAddr(10, true, true, 5);
  constexpr auto garbage =
      smesh::makeLocalAddr(smesh::kLocalAddrGarbageMask | 3u);
  constexpr auto sp_sum = smesh::add_with_overflow(smesh::makeSpAddr(14), 3);
  constexpr auto acc_sum =
      smesh::add_with_overflow(smesh::makeAccAddr(14, true, true, 5), 3);

  return smesh::kSpAddrBits == 4 && smesh::kAccAddrBits == 4 &&
         smesh::kLocalAddrDataBits == 4 &&
         !sp.is_acc_addr() && sp.sp_bank() == 1 && sp.sp_row() == 2 &&
         sp.full_sp_addr() == 6 &&
         acc.is_acc_addr() && acc.acc_bank() == 1 && acc.acc_row() == 2 &&
         acc.full_acc_addr() == 10 && acc.accumulate() &&
         acc.read_full_acc_row() && acc.norm_cmd() == 5 &&
         garbage.is_garbage() &&
         sp_sum.overflow && sp_sum.addr.full_sp_addr() == 1 &&
         !sp_sum.addr.is_acc_addr() &&
         acc_sum.overflow && acc_sum.addr.full_acc_addr() == 1 &&
         acc_sum.addr.is_acc_addr() && acc_sum.addr.accumulate() &&
         acc_sum.addr.read_full_acc_row() && acc_sum.addr.norm_cmd() == 5;
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
  const auto& config_entry = rs.loadEntry();
  if (config_entry.q != smesh::SmeshQueueClass::Load ||
      !config_entry.is_config || !config_entry.complete_on_issue ||
      config_entry.allocated_at != 0) {
    return false;
  }
  if (!rs.complete(rs.entry().rob_id)) {
    return false;
  }

  constexpr auto base = smesh::makeSpAddr(0);
  constexpr smesh::MatrixShape shape{2, 12};
  const auto load = command(
      smesh::SmeshFunct::Mvin2,
      0,
      smesh::packLocal(base, shape));
  if (!rs.allocate(load)) {
    return false;
  }

  const auto& entry = rs.loadEntry();
  return entry.q == smesh::SmeshQueueClass::Load && !entry.is_config &&
         !entry.complete_on_issue && entry.opa.valid && entry.opa_is_dst &&
         !entry.opb.valid && entry.allocated_at == 1 &&
         entry.opa.bits.start.raw == base.raw &&
         entry.opa.bits.end.raw == (base + 10).raw &&
         !entry.opa.bits.wraps_around;
}

bool testStoreRange() {
  smesh::SmeshRS rs;

  constexpr auto base = smesh::makeAccAddr(0);
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
         entry.opa.bits.start.raw == base.raw &&
         entry.opa.bits.end.raw == (base + 10).raw &&
         !entry.opa.bits.wraps_around;
}

bool testStoreSpadRange() {
  smesh::SmeshRS rs;

  constexpr auto destination_base = smesh::makeSpAddr(8);
  constexpr auto source_base = smesh::makeAccAddr(4);
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
         entry.opa.bits.start.raw == destination_base.raw &&
         entry.opa.bits.end.raw ==
             (destination_base + static_cast<std::uint32_t>(shape.rows)).raw &&
         !entry.opa.bits.wraps_around &&
         entry.opb.bits.start.raw == source_base.raw &&
         entry.opb.bits.end.raw ==
             (source_base + static_cast<std::uint32_t>(shape.rows)).raw &&
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
  const auto& config_entry = rs.executeEntry();
  if (config_entry.q != smesh::SmeshQueueClass::Execute ||
      !config_entry.is_config || config_entry.complete_on_issue) {
    return false;
  }
  if (!rs.complete(rs.entry().rob_id)) {
    return false;
  }

  constexpr auto source_base = smesh::makeSpAddr(0);
  constexpr auto destination_base = smesh::makeAccAddr(8);
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
         entry.opa.bits.start.raw == destination_base.raw &&
         entry.opa.bits.end.raw == (destination_base + 6).raw &&
         !entry.opa.bits.wraps_around &&
         entry.opb.bits.start.raw == source_base.raw &&
         entry.opb.bits.end.raw ==
             (source_base + static_cast<std::uint32_t>(source_shape.rows)).raw &&
         !entry.opb.bits.wraps_around;
}

bool testComputeRange() {
  constexpr auto a_base = smesh::makeSpAddr(0);
  constexpr auto bd_base = smesh::makeSpAddr(8);
  constexpr smesh::MatrixShape a_shape{2, smesh::kDim};
  constexpr smesh::MatrixShape bd_shape{smesh::kDim, smesh::kDim};

  smesh::SmeshRS flip_rs;
  const auto flip_config = command(
      smesh::SmeshFunct::Config,
      smesh::packConfigExecuteRs1(2, false),
      smesh::packConfigExecuteRs2(1));
  if (!flip_rs.allocate(flip_config) ||
      !flip_rs.complete(flip_rs.entry().rob_id)) {
    return false;
  }
  const auto flip = command(
      smesh::SmeshFunct::ComputeFlip,
      smesh::packLocal(a_base, a_shape),
      smesh::packLocal(bd_base, bd_shape));
  if (!flip_rs.allocate(flip)) {
    return false;
  }
  const auto& flip_entry = flip_rs.executeEntry();
  const bool flip_ok =
      flip_entry.opa.valid && !flip_entry.opa_is_dst && flip_entry.opb.valid &&
      flip_entry.opa.bits.start.raw == a_base.raw &&
      flip_entry.opa.bits.end.raw == (a_base + 4).raw &&
      flip_entry.opb.bits.start.raw == bd_base.raw &&
      flip_entry.opb.bits.end.raw ==
          (bd_base + static_cast<std::uint32_t>(bd_shape.rows)).raw;

  smesh::SmeshRS stay_rs;
  const auto stay_config = command(
      smesh::SmeshFunct::Config,
      smesh::packConfigExecuteRs1(2, true),
      smesh::packConfigExecuteRs2(1));
  if (!stay_rs.allocate(stay_config) ||
      !stay_rs.complete(stay_rs.entry().rob_id)) {
    return false;
  }
  const auto stay = command(
      smesh::SmeshFunct::ComputeStay,
      smesh::packLocal(a_base, a_shape),
      smesh::packLocal(bd_base, bd_shape));
  if (!stay_rs.allocate(stay)) {
    return false;
  }
  const auto& stay_entry = stay_rs.executeEntry();
  const bool stay_ok =
      stay_entry.opa.valid && !stay_entry.opa_is_dst && stay_entry.opb.valid &&
      stay_entry.opa.bits.start.raw == a_base.raw &&
      stay_entry.opa.bits.end.raw ==
          (a_base + static_cast<std::uint32_t>(2 * smesh::kDim)).raw &&
      stay_entry.opb.bits.start.raw == bd_base.raw &&
      stay_entry.opb.bits.end.raw ==
          (bd_base + static_cast<std::uint32_t>(bd_shape.rows)).raw;

  return flip_ok && stay_ok;
}

} // namespace

int main() {
  const bool local_addr_ok = testLocalAddr();
  const bool load_ok = testLoadRange();
  const bool store_ok = testStoreRange();
  const bool store_spad_ok = testStoreSpadRange();
  const bool preload_ok = testPreloadRange();
  const bool compute_ok = testComputeRange();
  std::printf("[SMESH_RS] %s local_addr\n",
              local_addr_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s load_range\n", load_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s store_range\n", store_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s store_spad_range\n",
              store_spad_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s preload_range\n",
              preload_ok ? "PASS" : "FAIL");
  std::printf("[SMESH_RS] %s compute_range\n",
              compute_ok ? "PASS" : "FAIL");
  return (local_addr_ok && load_ok && store_ok && store_spad_ok && preload_ok &&
          compute_ok)
             ? 0
             : 1;
}
