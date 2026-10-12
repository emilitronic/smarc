// **********************************************************************
// smesh/src/loop_conv/LoopConvUtilization.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 12 2026

#include "LoopConvUtilization.hpp"
#include <cassert>
#include <limits>

namespace smesh {
namespace {

unsigned nextCount(const char* group, unsigned count, bool issued, unsigned completed, std::size_t limit) {
  // Original requires completions to refer to commands outstanding BEFORE
  // this cycle; a simultaneous issue cannot cover an otherwise invalid return.
  assert_always(completed <= count, "LoopConv %s utilization underflow: count=%u completed=%u", group, count, completed);
  const auto next = count + unsigned(issued) - completed;
  assert_always(next <= limit, "LoopConv %s utilization exceeds capacity: next=%u limit=%u", group, next, unsigned(limit));
  return next;
}

} // namespace

LoopConvUtilization::LoopConvUtilization(std::string /*name*/, IMPL_CTOR) {
  static_assert(kDefaultConfig.rs_load_entries > 0 && kDefaultConfig.rs_execute_entries > 0 &&
                kDefaultConfig.rs_store_entries > 0, "LoopConv requires nonzero RS capacities");
  static_assert(kDefaultConfig.rs_load_entries <= std::numeric_limits<std::uint16_t>::max() &&
                kDefaultConfig.rs_execute_entries <= std::numeric_limits<std::uint16_t>::max() &&
                kDefaultConfig.rs_store_entries <= std::numeric_limits<std::uint16_t>::max(),
                "LoopConv utilization counters must fit in u16");
  ld_count_Q_ <= ld_count_D_;
  ex_count_Q_ <= ex_count_D_;
  st_count_Q_ <= st_count_D_;
  UPDATE(updateStatus).reads(ld_count_Q_, ex_count_Q_, st_count_Q_)
      .writes(ld_outstanding, ex_outstanding, st_outstanding)
      .writes(ld_utilization_at_limit, ex_utilization_at_limit, st_utilization_at_limit);
  UPDATE(updateNextState).reads(ld_count_Q_, ex_count_Q_, st_count_Q_)
      .reads(ld_cmd_fire, ex_cmd_fire, st_cmd_fire, ld_completed, ex_completed, st_completed)
      .writes(ld_count_D_, ex_count_D_, st_count_D_);
}

// Throttle workers using current registered counts, not this cycle's returns.
void LoopConvUtilization::updateStatus() {
  ld_outstanding = *ld_count_Q_;
  ex_outstanding = *ex_count_Q_;
  st_outstanding = *st_count_Q_;
  ld_utilization_at_limit = bit(ld_count_Q_ >= kDefaultConfig.rs_load_entries);
  ex_utilization_at_limit = bit(ex_count_Q_ >= kDefaultConfig.rs_execute_entries);
  st_utilization_at_limit = bit(st_count_Q_ >= kDefaultConfig.rs_store_entries);
}

// Apply accepted commands and returned RS credits together at the next edge.
void LoopConvUtilization::updateNextState() {
  const auto ld = nextCount("load", unsigned(*ld_count_Q_), ld_cmd_fire == 1, unsigned(*ld_completed), kDefaultConfig.rs_load_entries);
  const auto ex = nextCount("execute", unsigned(*ex_count_Q_), ex_cmd_fire == 1, unsigned(*ex_completed), kDefaultConfig.rs_execute_entries);
  const auto st = nextCount("store", unsigned(*st_count_Q_), st_cmd_fire == 1, unsigned(*st_completed), kDefaultConfig.rs_store_entries);
  if (ld != ld_count_Q_ || ex != ex_count_Q_ || st != st_count_Q_) {
    trace("loop_conv utilization: next ld=%u ex=%u st=%u returns={%u,%u,%u}\n",
          ld, ex, st, unsigned(*ld_completed), unsigned(*ex_completed), unsigned(*st_completed));
  }
  if (ld != ld_count_Q_) ld_count_D_ = ld;
  if (ex != ex_count_Q_) ex_count_D_ = ex;
  if (st != st_count_Q_) st_count_D_ = st;
}

void LoopConvUtilization::reset() {
  ld_count_D_.reset(0); ex_count_D_.reset(0); st_count_D_.reset(0);
  ld_outstanding.reset(0); ex_outstanding.reset(0); st_outstanding.reset(0);
  ld_utilization_at_limit.reset(0); ex_utilization_at_limit.reset(0); st_utilization_at_limit.reset(0);
}

} // namespace smesh
