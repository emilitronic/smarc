// **********************************************************************
// smesh/include/ExCtrlWriteback.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026
/*
Routes mesh responses to bank-local scratchpad/accumulator writes and execute
completion bookkeeping.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include "Mesher.hpp"
#include "SmeshPorts.hpp"

#include <cstdint>

namespace smesh {

class ExCtrlWriteback : public Component {
  DECLARE_COMPONENT(ExCtrlWriteback);

 public:
  ExCtrlWriteback(std::string name, COMPONENT_CTOR);

  Clock(clk);

  Input(bit, mesh_resp_val);           // mesh has response bits available
  Input(MesherResp, mesh_resp_bits);   // mesh response payload

  Input(u8, current_dataflow);         // execute dataflow config register
  Input(u32, c_addr_stride);           // C local-address stride from CONFIG_EX
  Input(u8, activation);               // activation config, TODO: decode/apply
  Input(u32, aligned_to);              // alignment setting used by output layout
  Input(bit, ex_write_to_spad);        // hardware config permits ExC writes to spad
  Input(bit, ex_write_to_acc);         // hardware config permits ExC writes to accumulator

  InputArray(bit, spad_write_rdy, kSpBanks);    // selected spad bank can accept exwrite
  OutputArray(bit, spad_write_val, kSpBanks);   // ExC writes selected spad bank
  OutputArray(SpadBankWriteReq, spad_write_bits, kSpBanks);

  InputArray(bit, accum_write_rdy, kAccBanks);  // selected accum bank can accept exwrite
  OutputArray(bit, accum_write_val, kAccBanks); // ExC writes selected accum bank
  OutputArray(AccumBankWriteReq, accum_write_bits, kAccBanks);

  Output(bit, mesh_completed_rs_tag_fire); // mesh response produced a completion event
  Output(bit, completed_val);          // execute completion valid
  Output(SmeshRsTag, completed_bits);  // execute completion tag

  void updateView();
  void updateState();
  void reset();

 private:
  std::uint32_t output_counter_ = 0; // current mesh-output row counter
};

} // namespace smesh
