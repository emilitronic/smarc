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
// Match Gemmini's local-address RAW check: 
// same memory space (check is_acc_addr field)
// same row address  (check data fields)
bool isSameAddress(SmeshLocalAddr lhs, SmeshLocalAddr rhs) {
  return lhs.is_acc_addr() == rhs.is_acc_addr() &&
         lhs.data() == rhs.data();
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
             ex_write_to_spad,
             tags_in_progress)
      .writes(functs,
              rs1s,
              rs2s,
              do_config,
              do_computes,
              do_preloads,
              in_prop)
      .writes(preload_cmd_place)
      .writes(a_address_rs1,
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
              ws_no_transpose,
              raw_hazards_are_impossible,
              raw_hazard_pre,
              raw_hazard_mulpre)
      .writes(third_instruction_needed,
              matmul_in_progress);
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
  in_prop   = bit(head_val[0] != 0 && funct[0] == SmeshFunct::ComputeFlip);

  const std::uint8_t preload_place = do_preloads[0] != 0 ? 0 : 1; // is PRELOAD in cmd(0) or cmd(1)? (cmd(2) is never PRELOAD)
  const bool dataflow_os     = current_dataflow == kExDataflowOS;
  const bool dataflow_ws     = current_dataflow == kExDataflowWS;
  const bool a_to_transposer = dataflow_os ? a_transpose == 0 : a_transpose != 0;
  const bool b_to_transposer = dataflow_os && bd_transpose != 0;
  const bool d_to_transposer = dataflow_ws && bd_transpose != 0;
  // a_address_place = a_place which cmd slot loads A operand, 0, 1, or 2
  // b_address_place = b_place which cmd slot loads B operand, 0, 1, or 2
  const std::uint8_t a_place = preload_place == 0 ? 1 : (a_to_transposer ? 2 : 0); // determine place of A operand in cmd queue head (depends on sensed code seq)
  const std::uint8_t b_place = preload_place == 0 ? 1 : (b_to_transposer ? 2 : 0); // determine place of B operand in cmd queue head (depends on sensed code seq)
  // determine place of preload command and addresses of operands
  preload_cmd_place = preload_place;

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
  b_rows = b_to_transposer  ? b_cols_default : b_rows_default;
  b_cols = b_to_transposer  ? b_rows_default : b_cols_default;
  d_rows = d_to_transposer  ? d_cols_default : d_rows_default;
  d_cols = d_to_transposer  ? d_rows_default : d_cols_default;
  c_rows = rowsOf(c_rs2);
  c_cols = colsOf(c_rs2);

  a_should_be_fed_into_transposer = bit(a_to_transposer);
  b_should_be_fed_into_transposer = bit(b_to_transposer);
  d_should_be_fed_into_transposer = bit(d_to_transposer);
  ws_no_transpose = bit(dataflow_ws && !a_to_transposer && !b_to_transposer && !d_to_transposer);

  bool any_matmul_in_progress = false;
  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    any_matmul_in_progress = any_matmul_in_progress || (*tags_in_progress[i]).rs_tag_valid != 0;
  }
  matmul_in_progress = bit(any_matmul_in_progress);
  // **** RAW Hazard detection logic ****
  // 1) Disable condition
  // If Ex never reads from accum & never writes to spad, then RAW hazard class can't happen
  const bool raw_hazards_impossible = ex_read_from_acc == 0 && 
                                      ex_write_to_spad == 0;
  raw_hazards_are_impossible = bit(raw_hazards_impossible);
  // TODO: compute from mesh tags_in_progress addresses.
  // 2) Preload hazard detection used when cmd(0)=PRELOAD (a single preload case).
  // Generally (not semantically as in "B vs C") compares older in-flight mesh output address against:
  // cmd(0).rs1 - PRELOAD's read B addr may conflict with mesh's queued write addr
  // cmd(1).rs1 - COMPUTE's read A addr may conflict with mesh's queued write addr
  // cmd(1).rs2 - COMPUTE's read B addr may conflict with mesh's queued write addr
  bool next_raw_hazard_pre = false;
  // 3) Mul/Preload hazard detection used when cmd(0)=COMPUTE and cmd(1)=PRELOAD
  // Compares older in-flight mesh output address agains:
  // cmd(1).rs1 - PRELOAD's rs1 is B addr to read
  // cmd(2).rs1 - COMPUTE's rs1 is A addr to read
  // cmd(2).rs2 - COMPUTE's rs2 is B addr to read
  bool next_raw_hazard_mulpre = false;

  for (std::size_t i = 0; i < kRsExecuteEntries; ++i) {
    const auto tag = *tags_in_progress[i];
    if (tag.addr.is_garbage() || raw_hazards_impossible) {
      continue;
    }

    const bool pre_raw_haz = isSameAddress(tag.addr, addrOf(rs1[0]));
    const bool mul_raw_haz = isSameAddress(tag.addr, addrOf(rs1[1])) || isSameAddress(tag.addr, addrOf(rs2[1]));
    next_raw_hazard_pre = next_raw_hazard_pre || pre_raw_haz || mul_raw_haz;

    const bool pre_raw_haz_mulpre = isSameAddress(tag.addr, addrOf(rs1[1]));
    const bool mul_raw_haz_mulpre = isSameAddress(tag.addr, addrOf(rs1[2])) || isSameAddress(tag.addr, addrOf(rs2[2]));
    next_raw_hazard_mulpre = next_raw_hazard_mulpre || pre_raw_haz_mulpre || mul_raw_haz_mulpre;
  }
  raw_hazard_pre    = bit(next_raw_hazard_pre);
  raw_hazard_mulpre = bit(next_raw_hazard_mulpre);
  // 4) Third instruction needed detection
  third_instruction_needed = bit(a_place > 1 || b_place > 1 || preload_place > 1 ||
                                 !raw_hazards_impossible);
}

} // namespace smesh
