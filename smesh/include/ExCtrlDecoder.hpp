// **********************************************************************
// smesh/include/ExCtrlDecoder.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Combinational execute-controller command decoder.
*/

#pragma once

#include <cascade/Cascade.hpp>

#include <cstdint>

#include "ExCtrlQueues.hpp"
#include "SmeshLocalAddr.hpp"
#include "SmeshPorts.hpp"

namespace smesh {

constexpr std::uint8_t kExDataflowWS = 0;
constexpr std::uint8_t kExDataflowOS = 1;

class ExCtrlDecoder : public Component {
  DECLARE_COMPONENT(ExCtrlDecoder);

 public:
  ExCtrlDecoder(std::string name, COMPONENT_CTOR);

  Clock(clk);

  InputArray(bit, head_val, kExCtrlCmdWindow);
  InputArray(SmeshIssue, head_bits, kExCtrlCmdWindow);

  Input(u8, current_dataflow);
  Input(bit, a_transpose);
  Input(bit, bd_transpose);
  Input(bit, raw_hazards_are_impossible_in);

  OutputArray(u32, functs, kExCtrlCmdWindow);
  OutputArray(u64, rs1s, kExCtrlCmdWindow);
  OutputArray(u64, rs2s, kExCtrlCmdWindow);

  Output(bit, do_config);
  OutputArray(bit, do_computes, kExCtrlCmdWindow);
  OutputArray(bit, do_preloads, kExCtrlCmdWindow);

  Output(u8, preload_cmd_place);
  Output(u8, a_address_place);
  Output(u8, b_address_place);

  Output(SmeshLocalAddr, a_address_rs1);
  Output(SmeshLocalAddr, b_address_rs2);
  Output(SmeshLocalAddr, d_address_rs1);
  Output(SmeshLocalAddr, c_address_rs2);

  Output(bit, multiply_garbage);
  Output(bit, accumulate_zeros);
  Output(bit, preload_zeros);

  Output(u16, a_rows);
  Output(u16, a_cols);
  Output(u16, b_rows);
  Output(u16, b_cols);
  Output(u16, d_rows);
  Output(u16, d_cols);
  Output(u16, c_rows);
  Output(u16, c_cols);

  Output(bit, a_should_be_fed_into_transposer);
  Output(bit, b_should_be_fed_into_transposer);
  Output(bit, d_should_be_fed_into_transposer);

  Output(bit, raw_hazards_are_impossible);
  Output(bit, raw_hazard_pre);
  Output(bit, raw_hazard_mulpre);
  Output(bit, third_instruction_needed);

  void update();
};

} // namespace smesh
