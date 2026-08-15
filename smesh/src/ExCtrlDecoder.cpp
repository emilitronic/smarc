// **********************************************************************
// smesh/src/ExCtrlDecoder.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 26 2026
/*
Combinational execute-controller command decoder implementation.
*/

#include "ExCtrlDecoder.hpp"

#include "SmeshCommand.hpp"

#include <algorithm>
#include <cstdint>

namespace smesh {

namespace {

// Return command rs1 as an ordinary C++ integer for decoder helper logic
std::uint64_t rawRs1(const SmeshIssue& issue) {
  return static_cast<std::uint64_t>(issue.cmd.rs1);
}
// Return command rs2 as an ordinary C++ integer for decoder helper logic
std::uint64_t rawRs2(const SmeshIssue& issue) {
  return static_cast<std::uint64_t>(issue.cmd.rs2);
}
// Extract row count from our packed local-operand encoding
std::uint16_t rowsOf(std::uint64_t packed) {
  return static_cast<std::uint16_t>(unpackLocal(packed).shape.rows);
}
// Extract column count from our packed local-operand encoding
std::uint16_t colsOf(std::uint64_t packed) {
  return static_cast<std::uint16_t>(unpackLocal(packed).shape.cols);
}
// Extract local address from our packed local-operand encoding
SmeshLocalAddr addrOf(std::uint64_t packed) {
  return makeLocalAddr(unpackLocal(packed).row);
}
// Interpret command funct field as the Smesh command enum
SmeshFunct functOf(const SmeshIssue& issue) {
  return static_cast<SmeshFunct>(static_cast<std::uint32_t>(issue.cmd.funct));
}
// True for either execute-side compute primitive
bool isCompute(SmeshFunct funct) {
  return funct == SmeshFunct::ComputeFlip || funct == SmeshFunct::ComputeStay;
}

} // namespace

ExCtrlDecoder::ExCtrlDecoder(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(head_val,
             head_bits,
             current_dataflow,
             a_transpose,
             bd_transpose,
             ex_read_from_acc,
             ex_write_to_spad)
      .writes(functs,
              rs1s,
              rs2s,
              do_config,
              do_computes,
              do_preloads,
              preload_cmd_place,
              a_address_place)
      .writes(b_address_place,
              a_address_rs1,
              b_address_rs2,
              d_address_rs1,
              c_address_rs2,
              multiply_garbage,
              accumulate_zeros,
              preload_zeros)
      .writes(a_rows, a_cols,
              b_rows, b_cols,
              d_rows, d_cols,
              c_rows, c_cols)
      .writes(a_should_be_fed_into_transposer,
              b_should_be_fed_into_transposer,
              d_should_be_fed_into_transposer,
              raw_hazards_are_impossible,
              raw_hazard_pre,
              raw_hazard_mulpre,
              third_instruction_needed);
}

void ExCtrlDecoder::update() {
  std::array<SmeshIssue,    kExCtrlCmdWindow> issue{}; // cmds im cmd queue
  std::array<std::uint64_t, kExCtrlCmdWindow> rs1{};   // cmd's operand
  std::array<std::uint64_t, kExCtrlCmdWindow> rs2{};   // cmd's operand
  std::array<SmeshFunct,    kExCtrlCmdWindow> funct{}; // cmd's funct field
  // scan cmd queue head and extract funct/rs1/rs2 & produce per-slot decode signals
  for (std::size_t i = 0; i < kExCtrlCmdWindow; ++i) {
    issue[i] = head_val[i] != 0 ? *head_bits[i] : SmeshIssue{};
    rs1[i]   = rawRs1(issue[i]);
    rs2[i]   = rawRs2(issue[i]);
    funct[i] = functOf(issue[i]);
    // funnel decoded values to decoder outputs
    functs[i] = static_cast<std::uint32_t>(funct[i]);
    rs1s[i]   = rs1[i];
    rs2s[i]   = rs2[i];
    do_computes[i] = bit(head_val[i] != 0 && isCompute(funct[i]));
    do_preloads[i] = bit(head_val[i] != 0 && funct[i] == SmeshFunct::Preload);
  }
  // check if 1st cmd is config cmd, and if so, set do_config output
  do_config = bit(head_val[0] != 0 && funct[0] == SmeshFunct::Config);

  const std::uint8_t preload_place = do_preloads[0] != 0 ? 0 : 1;
  const bool dataflow_os     = current_dataflow == kExDataflowOS;
  const bool dataflow_ws     = current_dataflow == kExDataflowWS;
  const bool a_to_transposer = dataflow_os ? a_transpose == 0 : a_transpose != 0;
  const bool b_to_transposer = dataflow_os && bd_transpose != 0;
  const bool d_to_transposer = dataflow_ws && bd_transpose != 0;
  const std::uint8_t a_place = preload_place == 0 ? 1 : (a_to_transposer ? 2 : 0); // determine place of A operand in cmd queue head (depends on sensed code seq)
  const std::uint8_t b_place = preload_place == 0 ? 1 : (b_to_transposer ? 2 : 0); // determine place of B operand in cmd queue head (depends on sensed code seq)
  // determine place of preload command and addresses of operands
  preload_cmd_place = preload_place;
  a_address_place = a_place; // loc of A operand in cmd queue head (depends on sensed code seq)
  b_address_place = b_place; // loc of B operand in cmd queue head (depends on sensed code seq)

  const auto a_rs1 = rs1[a_place];
  const auto b_rs2 = rs2[b_place];
  const auto d_rs1 = rs1[preload_place];
  const auto c_rs2 = rs2[preload_place];

  a_address_rs1 = addrOf(a_rs1);
  b_address_rs2 = addrOf(b_rs2);
  d_address_rs1 = addrOf(d_rs1);
  c_address_rs2 = addrOf(c_rs2);

  multiply_garbage = bit(addrOf(a_rs1).is_garbage());
  accumulate_zeros = bit(addrOf(b_rs2).is_garbage());
  preload_zeros    = bit(addrOf(d_rs1).is_garbage());

  const auto a_rows_default = rowsOf(a_rs1);
  const auto a_cols_default = colsOf(a_rs1);
  const auto b_rows_default = rowsOf(b_rs2);
  const auto b_cols_default = colsOf(b_rs2);
  const auto d_rows_default = rowsOf(d_rs1);
  const auto d_cols_default = colsOf(d_rs1);

  a_rows = a_transpose != 0 ? a_cols_default : a_rows_default;
  a_cols = a_transpose != 0 ? a_rows_default : a_cols_default;
  b_rows = b_to_transposer ? b_cols_default : b_rows_default;
  b_cols = b_to_transposer ? b_rows_default : b_cols_default;
  d_rows = d_to_transposer ? d_cols_default : d_rows_default;
  d_cols = d_to_transposer ? d_rows_default : d_cols_default;
  c_rows = rowsOf(c_rs2);
  c_cols = colsOf(c_rs2);

  a_should_be_fed_into_transposer = bit(a_to_transposer);
  b_should_be_fed_into_transposer = bit(b_to_transposer);
  d_should_be_fed_into_transposer = bit(d_to_transposer);

  const bool raw_hazards_impossible = ex_read_from_acc == 0 && ex_write_to_spad == 0;
  raw_hazards_are_impossible = bit(raw_hazards_impossible);
  // TODO: compute from mesh tags_in_progress addresses.
  raw_hazard_pre = 0;
  // TODO: compute from mesh tags_in_progress addresses.
  raw_hazard_mulpre = 0;
  third_instruction_needed = bit(a_place > 1 ||
                                b_place > 1 ||
                                preload_place > 1 ||
                                !raw_hazards_impossible);
}

} // namespace smesh
