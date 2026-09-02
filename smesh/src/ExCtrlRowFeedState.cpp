// **********************************************************************
// smesh/src/ExCtrlRowFeedState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlRowFeedState.hpp"

namespace smesh {

namespace {

// Increment modulo max_plus_one while avoiding unsigned underflow when it is zero.
std::uint32_t wrappingIncrement(std::uint32_t value, std::uint32_t max_plus_one) {
  if (max_plus_one == 0) {
    return 0;
  }
  return value >= max_plus_one - 1 ? 0 : value + 1;
}

} // namespace

TraceKey(row_feed_);

ExCtrlRowFeedState::ExCtrlRowFeedState(std::string /*name*/, IMPL_CTOR) {
  // Registered outputs are driven and updated structurally
  a_fire_counter        <= a_fire_counter_reg_;
  b_fire_counter        <= b_fire_counter_reg_;
  d_fire_counter        <= d_fire_counter_reg_;
  a_fire_started        <= a_fire_started_reg_;
  b_fire_started        <= b_fire_started_reg_;
  d_fire_started        <= d_fire_started_reg_;
  a_addr_offset         <= a_addr_offset_reg_;
  mul_pre_counter_sub   <= mul_pre_counter_sub_reg_;
  mul_pre_counter_count <= mul_pre_counter_count_reg_;
  mul_pre_counter_lock  <= mul_pre_counter_lock_reg_;
  preload_zero_counter  <= preload_zero_counter_reg_;

  UPDATE(updateStatus)
      .reads(a_fire_counter, b_fire_counter, d_fire_counter,
             a_fire_started, b_fire_started, d_fire_started,
             a_addr_offset)
      .reads(a_fire, b_fire, d_fire, total_rows, cntl_rdy)
      .writes(first, about_to_fire_all_rows);
  UPDATE(updateNextState)
      .reads(firing, a_fire, b_fire, d_fire,
             total_rows, a_addr_stride, cntl_rdy)
      .reads(a_fire_counter, b_fire_counter, d_fire_counter,
             a_fire_started, b_fire_started, d_fire_started,
             a_addr_offset)
      .writes(a_fire_counter_reg_, b_fire_counter_reg_, d_fire_counter_reg_,
              a_fire_started_reg_, b_fire_started_reg_, d_fire_started_reg_,
              a_addr_offset_reg_);
}

// Compute remaining status o/p: first & about_to_fire_all_rows
void ExCtrlRowFeedState::updateStatus() {
  const auto rows      = static_cast<std::uint32_t>(*total_rows);
  const auto a_counter = static_cast<std::uint32_t>(*a_fire_counter);
  const auto b_counter = static_cast<std::uint32_t>(*b_fire_counter);
  const auto d_counter = static_cast<std::uint32_t>(*d_fire_counter);
  // total_rows is nonzero for real operations; guard the subtraction for C++.
  const bool a_on_last = rows != 0 && a_counter == rows - 1;
  const bool b_on_last = rows != 0 && b_counter == rows - 1;
  const bool d_on_last = rows != 0 && d_counter == rows - 1;
  const bool finishing =
      ((a_on_last && a_fire != 0) || a_counter == 0) &&
      ((b_on_last && b_fire != 0) || b_counter == 0) &&
      ((d_on_last && d_fire != 0) || d_counter == 0) &&
      (a_fire_started != 0 || b_fire_started != 0 || d_fire_started != 0) &&
      cntl_rdy != 0;

  first = bit(a_fire_started == 0 && b_fire_started == 0 && d_fire_started == 0);
  about_to_fire_all_rows = bit(finishing);

  trace(row_feed_,
        "fire{%u%u%u} cnt{%u,%u,%u} started{%u%u%u} offset=%u rows=%u cntl_rdy=%u last=%u\n",
        static_cast<unsigned>(a_fire != 0),
        static_cast<unsigned>(b_fire != 0),
        static_cast<unsigned>(d_fire != 0),
        a_counter,
        b_counter,
        d_counter,
        static_cast<unsigned>(a_fire_started != 0),
        static_cast<unsigned>(b_fire_started != 0),
        static_cast<unsigned>(d_fire_started != 0),
        static_cast<unsigned>(*a_addr_offset),
        rows,
        static_cast<unsigned>(cntl_rdy != 0),
        static_cast<unsigned>(finishing));
}

// Compute the row-feed state values to store for the next cycle
void ExCtrlRowFeedState::updateNextState() {
  const bool active = firing != 0;
  const bool control_ready = cntl_rdy != 0;
  const auto rows = static_cast<std::uint32_t>(*total_rows);
  const auto a_counter = static_cast<std::uint32_t>(*a_fire_counter);
  const auto b_counter = static_cast<std::uint32_t>(*b_fire_counter);
  const auto d_counter = static_cast<std::uint32_t>(*d_fire_counter);
  const bool a_finishes =
      ((rows != 0 && a_counter == rows - 1 && a_fire != 0) || a_counter == 0);
  const bool b_finishes =
      ((rows != 0 && b_counter == rows - 1 && b_fire != 0) || b_counter == 0);
  const bool d_finishes =
      ((rows != 0 && d_counter == rows - 1 && d_fire != 0) || d_counter == 0);
  const bool finishing = a_finishes && b_finishes && d_finishes &&
                         (a_fire_started != 0 || b_fire_started != 0 || d_fire_started != 0) &&
                         control_ready;

  if (!active) {
    a_fire_counter_reg_ = 0;
    a_addr_offset_reg_ = 0;
  } else if (a_fire != 0 && control_ready) {
    const bool a_on_last = rows != 0 && a_counter == rows - 1;
    a_fire_counter_reg_ = wrappingIncrement(a_counter, rows);
    a_addr_offset_reg_ = a_on_last
                             ? 0
                             : static_cast<std::uint32_t>(*a_addr_offset) +
                                   static_cast<std::uint32_t>(*a_addr_stride);
    a_fire_started_reg_ = 1;
  }

  if (!active) {
    b_fire_counter_reg_ = 0;
  } else if (b_fire != 0 && control_ready) {
    b_fire_counter_reg_ = wrappingIncrement(b_counter, rows);
    b_fire_started_reg_ = 1;
  }

  if (!active) {
    d_fire_counter_reg_ = 0;
  } else if (d_fire != 0 && control_ready) {
    d_fire_counter_reg_ = wrappingIncrement(d_counter, rows);
    d_fire_started_reg_ = 1;
  }

  // This later assignment wins over the started=true updates on the final beat.
  if (finishing) {
    a_fire_started_reg_ = 0;
    b_fire_started_reg_ = 0;
    d_fire_started_reg_ = 0;
  }
}

void ExCtrlRowFeedState::reset() {
  a_fire_counter_reg_.reset(0);
  b_fire_counter_reg_.reset(0);
  d_fire_counter_reg_.reset(0);
  a_fire_started_reg_.reset(0);
  b_fire_started_reg_.reset(0);
  d_fire_started_reg_.reset(0);
  a_addr_offset_reg_.reset(0);
  mul_pre_counter_sub_reg_.reset(0);
  mul_pre_counter_count_reg_.reset(0);
  mul_pre_counter_lock_reg_.reset(0);
  preload_zero_counter_reg_.reset(0);

  first.reset(1);
  about_to_fire_all_rows.reset(0);
}

} // namespace smesh
