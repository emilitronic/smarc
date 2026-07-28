// **********************************************************************
// smesh/src/ExCtrlRowFeedState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026

#include "ExCtrlRowFeedState.hpp"

namespace smesh {

ExCtrlRowFeedState::ExCtrlRowFeedState(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateView)
      .writes(a_fire_counter,
              b_fire_counter,
              d_fire_counter,
              a_fire_started,
              b_fire_started,
              d_fire_started,
              a_addr_offset,
              mul_pre_counter_sub)
      .writes(mul_pre_counter_count,
              mul_pre_counter_lock,
              preload_zero_counter,
              about_to_fire_all_rows);
}

void ExCtrlRowFeedState::updateView() {
  a_fire_counter = a_fire_counter_;
  b_fire_counter = b_fire_counter_;
  d_fire_counter = d_fire_counter_;
  a_fire_started = bit(a_fire_started_);
  b_fire_started = bit(b_fire_started_);
  d_fire_started = bit(d_fire_started_);
  a_addr_offset = a_addr_offset_;
  mul_pre_counter_sub = mul_pre_counter_sub_;
  mul_pre_counter_count = mul_pre_counter_count_;
  mul_pre_counter_lock = bit(mul_pre_counter_lock_);
  preload_zero_counter = preload_zero_counter_;
  about_to_fire_all_rows = 0;
}

void ExCtrlRowFeedState::reset() {
  a_fire_counter_ = 0;
  b_fire_counter_ = 0;
  d_fire_counter_ = 0;
  a_fire_started_ = false;
  b_fire_started_ = false;
  d_fire_started_ = false;
  a_addr_offset_ = 0;
  mul_pre_counter_sub_ = 0;
  mul_pre_counter_count_ = 0;
  mul_pre_counter_lock_ = false;
  preload_zero_counter_ = 0;

  a_fire_counter.reset(0);
  b_fire_counter.reset(0);
  d_fire_counter.reset(0);
  a_fire_started.reset(0);
  b_fire_started.reset(0);
  d_fire_started.reset(0);
  a_addr_offset.reset(0);
  mul_pre_counter_sub.reset(0);
  mul_pre_counter_count.reset(0);
  mul_pre_counter_lock.reset(0);
  preload_zero_counter.reset(0);
  about_to_fire_all_rows.reset(0);
}

} // namespace smesh
