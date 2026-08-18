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
  DECLARE_COMPONENT(ExCtrlRowFeedState);

 public:
  ExCtrlRowFeedState(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, firing); // any A/B/D row-feed stream is active this cycle
  Input(bit, a_fire); // A row-beat was accepted this cycle
  Input(bit, b_fire); // B row-beat was accepted this cycle
  Input(bit, d_fire); // D row-beat was accepted this cycle
  Input(u32, total_rows); // total row-beats for the active mesh request
  Input(u32, a_addr_stride); // CONFIG_EX A local-address stride

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

  void updateView();
  void reset();

 private:
  std::uint32_t a_fire_counter_ = 0;
  std::uint32_t b_fire_counter_ = 0;
  std::uint32_t d_fire_counter_ = 0;
  bool a_fire_started_ = false;
  bool b_fire_started_ = false;
  bool d_fire_started_ = false;
  std::uint32_t a_addr_offset_ = 0;
  std::uint32_t mul_pre_counter_sub_ = 0;
  std::uint32_t mul_pre_counter_count_ = 0;
  bool mul_pre_counter_lock_ = false;
  std::uint32_t preload_zero_counter_ = 0;

  // TODO: use fire signals, total_rows, and a_addr_stride to update row-feed progress state.
};

} // namespace smesh
