// **********************************************************************
// smesh/include/loop_matmul/LoopMatmulCmdArb.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 27 2026
/*
5-way command arbiter with priority order: StC > Ex > LdD > LdAB > StCSpad. 
Propagates ready only to the selected command.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "SmeshPorts.hpp"

namespace smesh {

// Fixed-priority arbiter for the five LoopMatmul command generators.
class LoopMatmulCmdArb : public Component {
  DECLARE_COMPONENT(LoopMatmulCmdArb);

 public:
  LoopMatmulCmdArb(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit,       st_c_val);
  Output(bit,      st_c_rdy);
  Input(SmeshCmd,  st_c_bits);

  Input(bit,       ex_val);
  Output(bit,      ex_rdy);
  Input(SmeshCmd,  ex_bits);

  Input(bit,       ld_d_val);
  Output(bit,      ld_d_rdy);
  Input(SmeshCmd,  ld_d_bits);

  Input(bit,       ld_ab_val);
  Output(bit,      ld_ab_rdy);
  Input(SmeshCmd,  ld_ab_bits);

  Input(bit,       st_c_spad_val);
  Output(bit,      st_c_spad_rdy);
  Input(SmeshCmd,  st_c_spad_bits);

  Output(bit,      out_val);
  Input(bit,       out_rdy);
  Output(SmeshCmd, out_bits);

  void updateOutput();
  void updateReady();
  void reset();

 private:
  Output(u8, selected_);
};

} // namespace smesh
