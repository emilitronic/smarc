// **********************************************************************
// smesh/include/LdCtrlCmdDec.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

// Reinterprets the load queue head without changing any controller state.
class LdCtrlCmdDec : public Component {
  DECLARE_COMPONENT(LdCtrlCmdDec);

 public:
  LdCtrlCmdDec(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,        head_val);
  Input(SmeshIssue, head_bits);

  Output(bit, do_config);
  Output(bit, do_load);
  Output(u8,  load_state_id);
  Output(u8,  config_state_id);
  Output(u8,  state_id);

  Output(u64,            vaddr);
  Output(SmeshLocalAddr, localaddr);
  Output(u32,            rows);
  Output(u32,            cols);

  Output(u64, config_stride);
  Output(u32, config_scale);
  Output(bit, config_shrink);
  Output(u16, config_block_stride);
  Output(u8,  config_pixel_repeats);

  void update();
};

} // namespace smesh
