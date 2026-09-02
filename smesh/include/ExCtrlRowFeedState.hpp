// **********************************************************************
// smesh/include/ExCtrlRowFeedState.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 28 2026
/*
Row-feed progress state for ExecuteController operand feeding.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include <cstdint>

namespace smesh {

class ExCtrlRowFeedState : public Component {
  DECLARE_COMPONENT(ExCtrlRowFeedState, RowFeed);

 public:
  ExCtrlRowFeedState(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, firing);        // any A/B/D row-feed stream is active this cycle
  Input(bit, a_fire);        // A row-beat was accepted this cycle
  Input(bit, b_fire);        // B row-beat was accepted this cycle
  Input(bit, d_fire);        // D row-beat was accepted this cycle
  Input(u32, total_rows);    // total row-beats for the active mesh request
  Input(u32, a_addr_stride); // CONFIG_EX A local-address stride
  Input(bit, cntl_rdy);      // MQ can accept the matching mesh-control packet

  Output(u32, a_fire_counter);       // number of A row-beats accepted so far
  Output(u32, b_fire_counter);       // number of B row-beats accepted so far
  Output(u32, d_fire_counter);       // number of D row-beats accepted so far
  Output(bit, a_fire_started);       // A row feeding has started for the active operation
  Output(bit, b_fire_started);       // B row feeding has started for the active operation
  Output(bit, d_fire_started);       // D row feeding has started for the active operation
  Output(bit, first);                // first mesh-control packet for this operation
  Output(u32, a_addr_offset);        // current A local-address offset
  Output(u32, mul_pre_counter_sub);  // TODO: refine when multiply+preload path is implemented
  Output(u32, mul_pre_counter_count);// TODO: refine when multiply+preload path is implemented
  Output(bit, mul_pre_counter_lock); // TODO: refine when multiply+preload path is implemented
  Output(u32, preload_zero_counter); // TODO: refine when preload-zero path is implemented
  Output(bit, about_to_fire_all_rows);// final row-beat for the active operation is about to fire

  void updateStatus();    // computes first/final-row combinational status
  void updateNextState(); // computes values written into the state registers
  void reset();

 private:
  Register(u32, a_fire_counter_reg_);
  Register(u32, b_fire_counter_reg_);
  Register(u32, d_fire_counter_reg_);
  Register(bit, a_fire_started_reg_);
  Register(bit, b_fire_started_reg_);
  Register(bit, d_fire_started_reg_);
  Register(u32, a_addr_offset_reg_);
  Register(u32, mul_pre_counter_sub_reg_);
  Register(u32, mul_pre_counter_count_reg_);
  Register(bit, mul_pre_counter_lock_reg_);
  Register(u32, preload_zero_counter_reg_);

  // TODO: complete the specialized row-feed counters when their real sources exist.
  // - mul_pre_counter_count/lock need performing_mul_pre and cntl_rdy; capture
  //   d_fire_counter on the first MQ stall, then lock until mul-pre ends.
  // - mul_pre_counter_sub needs performing_mul_pre and the im2col response's
  //   im2col_delay flag; load 2 on a delay and count back toward zero otherwise.
  // - preload_zero_counter needs mesh A/D data-valid signals plus the dequeued
  //   control packet's preload_zeros and operation-mode fields. Reassess whether
  //   that state belongs here or closer to the mesh-input/dequeue logic first.
  // Keep these registers reset to zero until those dependencies are connected.
};

} // namespace smesh
