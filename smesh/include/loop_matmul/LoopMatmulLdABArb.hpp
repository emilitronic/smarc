// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulLdABArb.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
Selects between LdA and LdB commands using static-weight rules (force, idle, 
and iterator selection) and propagates ready only to the the selected load.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

// Selects between LdA and LdB commands using Gemmini's static-weight rules.
class LoopMatmulLdABArb : public Component {
  DECLARE_COMPONENT(LoopMatmulLdABArb);

 public:
  LoopMatmulLdABArb(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,       a_val);
  Output(bit,      a_rdy);
  Input(SmeshCmd,  a_bits);
  Input(bit,       a_idle);
  Input(u16,       a_k);
  Input(u16,       a_i);
  Input(u8,        a_loop_id);

  Input(bit,       b_val);
  Output(bit,      b_rdy);
  Input(SmeshCmd,  b_bits);
  Input(bit,       b_idle);
  Input(u16,       b_k);
  Input(u16,       b_j);
  Input(u8,        b_loop_id);

  Input(u8,        head_loop_id);
  Input(bit,       is_resadd);

  Output(bit,      out_val);
  Input(bit,       out_rdy);
  Output(SmeshCmd, out_bits);

  void update();
  void reset();
};

} // namespace smesh
