// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulUtilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 13 2026

#include "LoopMatmulUtilization.hpp"

#include <cassert>
#include <limits>

namespace smesh {
namespace {

unsigned nextCount(const char* group, unsigned count, bool issued,
                   unsigned completed, std::size_t limit) {
  assert_always(completed <= count,
                "LoopMatmul %s utilization underflow: count=%u completed=%u",
                group, count, completed);
  const auto next = count + unsigned(issued) - completed;
  assert_always(next <= limit,
                "LoopMatmul %s utilization exceeds capacity: next=%u limit=%u",
                group, next, unsigned(limit));
  return next;
}

} // namespace

LoopMatmulUtilization::LoopMatmulUtilization(std::string /*name*/, IMPL_CTOR) {
  static_assert(kDefaultConfig.rs_load_entries > 0 &&
                kDefaultConfig.rs_execute_entries > 0 &&
                kDefaultConfig.rs_store_entries > 0,
                "LoopMatmul requires nonzero RS capacities");
  static_assert(kDefaultConfig.rs_load_entries <= std::numeric_limits<std::uint16_t>::max() &&
                kDefaultConfig.rs_execute_entries <= std::numeric_limits<std::uint16_t>::max() &&
                kDefaultConfig.rs_store_entries <= std::numeric_limits<std::uint16_t>::max(),
                "LoopMatmul utilization counters must fit in u16");

  ld_count_Q_ <= ld_count_D_;
  ex_count_Q_ <= ex_count_D_;
  st_count_Q_ <= st_count_D_;

  UPDATE(updateStatus)
      .reads(ld_count_Q_, ex_count_Q_, st_count_Q_)
      .writes(ld_outstanding, ex_outstanding, st_outstanding)
      .writes(ld_utilization_at_limit, ex_utilization_at_limit, st_utilization_at_limit);
  UPDATE(updateNextState)
      .reads(ld_count_Q_, lda_cmd_fire, ldb_cmd_fire, ldd_cmd_fire, ld_completed)
      .reads(ex_count_Q_, ex_cmd_fire, ex_completed)
      .reads(st_count_Q_, st_c_cmd_fire, st_c_spad_cmd_fire, st_completed)
      .writes(ld_count_D_, ex_count_D_, st_count_D_);
}

// Report registered outstanding counts and stop each worker group at its RS limit.
void LoopMatmulUtilization::updateStatus() {
  ld_outstanding = *ld_count_Q_;
  ex_outstanding = *ex_count_Q_;
  st_outstanding = *st_count_Q_;
  ld_utilization_at_limit = bit(ld_count_Q_ >= kDefaultConfig.rs_load_entries);
  ex_utilization_at_limit = bit(ex_count_Q_ >= kDefaultConfig.rs_execute_entries);
  st_utilization_at_limit = bit(st_count_Q_ >= kDefaultConfig.rs_store_entries);
}

// Apply accepted command and RS completion transfers to next-cycle counts.
void LoopMatmulUtilization::updateNextState() {
  // The global command arbiter allows at most one command from each group per cycle.
  const bool ld_issued = lda_cmd_fire == 1 || ldb_cmd_fire == 1 || ldd_cmd_fire == 1;
  const bool st_issued = st_c_cmd_fire == 1 || st_c_spad_cmd_fire == 1;
  const auto ld = nextCount("load", unsigned(*ld_count_Q_), ld_issued,
                            unsigned(*ld_completed), kDefaultConfig.rs_load_entries);
  const auto ex = nextCount("execute", unsigned(*ex_count_Q_), ex_cmd_fire == 1,
                            unsigned(*ex_completed), kDefaultConfig.rs_execute_entries);
  const auto st = nextCount("store", unsigned(*st_count_Q_), st_issued,
                            unsigned(*st_completed), kDefaultConfig.rs_store_entries);
  if (ld != ld_count_Q_ || ex != ex_count_Q_ || st != st_count_Q_) {
    trace("loop_matmul utilization: next ld=%u ex=%u st=%u returns={%u,%u,%u}\n",
          ld, ex, st, unsigned(*ld_completed), unsigned(*ex_completed),
          unsigned(*st_completed));
  }
  if (ld != ld_count_Q_) ld_count_D_ = static_cast<std::uint16_t>(ld);
  if (ex != ex_count_Q_) ex_count_D_ = static_cast<std::uint16_t>(ex);
  if (st != st_count_Q_) st_count_D_ = static_cast<std::uint16_t>(st);
}

void LoopMatmulUtilization::reset() {
  ld_count_D_.reset(0);
  ex_count_D_.reset(0);
  st_count_D_.reset(0);
  ld_outstanding.reset(0);
  ex_outstanding.reset(0);
  st_outstanding.reset(0);
  ld_utilization_at_limit.reset(0);
  ex_utilization_at_limit.reset(0);
  st_utilization_at_limit.reset(0);
}

} // namespace smesh
