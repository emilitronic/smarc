// **********************************************************************
// smesh/src/tb_ex_ctrl_scenarios.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 1 2026
/*
Test-only ExCtrl stimulus and expectation descriptions.
*/
#pragma once

#include "SmeshCommand.hpp"
#include "SmeshPorts.hpp"

#include <array>
#include <cstdint>

namespace smesh {
namespace tb {

// Expected early-pipeline addresses observed while the scenario runs.
struct ExCtrlExpected {
  // Early-pipeline observations: these are address/read-request checks, not
  // final matrix-result expectations.
  // A/B/D are the logical first row addresses selected by ExCtrl.
  std::uint32_t rowaddr_a_address  = 0;
  std::uint32_t rowaddr_b_address  = 0;
  std::uint32_t rowaddr_d_address  = 0;

  // The first bank-local scratchpad row request emitted by the read logic.
  std::uint32_t first_read_address = 0;
};

// One test program plus the observations expected from that program.
struct ExCtrlScenario {
  // A scenario is both the command stream presented as if it came from RS
  // and the observations we expect while that stream moves through ExCtrl.
  const char* name = "";
  std::array<SmeshIssue, 3> program{};
  ExCtrlExpected expected{};
};

// Build one RS-like command with a tag, opcode, and packed operands.
inline SmeshIssue makeScenarioIssue(SmeshRsTag tag,
                                    SmeshFunct funct,
                                    std::uint64_t rs1 = 0,
                                    std::uint64_t rs2 = 0,
                                    bool tag_valid = true) {
  SmeshIssue issue{};
  issue.rs_tag_valid = bit(tag_valid);
  issue.rs_tag = tag;
  issue.cmd.funct = static_cast<std::uint32_t>(funct);
  issue.cmd.rs1 = rs1;
  issue.cmd.rs2 = rs2;
  return issue;
}

// Create the basic CONFIG -> PRELOAD -> COMPUTE ExCtrl scenario.
inline ExCtrlScenario makeConfigPreloadComputeScenario() {
  ExCtrlScenario scenario{};
  scenario.name = "config_preload_compute";

  // The test deliberately uses a short, readable command sequence:
  //   1. CONFIG_EX: establish WS mode and unit address strides.
  //   2. PRELOAD:  read weights from SPAD row 4 and target C at ACC row 8.
  //   3. COMPUTE:  read A from SPAD row 12 and B from SPAD row 4.
  // Each command has a distinct RS tag so queue movement is easy to inspect.
  scenario.program = {
      makeScenarioIssue(7, SmeshFunct::Config,      packConfigExecuteRs1(1), packConfigExecuteRs2(1)),
      makeScenarioIssue(8, SmeshFunct::Preload,     packLocal(makeSpAddr(4),  {kDim, kDim}), packLocal(makeAccAddr(8), {kDim, kDim})),
      makeScenarioIssue(9, SmeshFunct::ComputeStay, packLocal(makeSpAddr(12), {kDim, kDim}), packLocal(makeSpAddr(4), {kDim, kDim})),
  };

  // The first row-address view is taken from the COMPUTE command. D is fed in
  // reverse row order, so its first address is row 4 + (DIM - 1) = 7.
  scenario.expected.rowaddr_a_address = 12;
  scenario.expected.rowaddr_b_address = 4;
  scenario.expected.rowaddr_d_address = 7;

  // The first real read request is the bank-local SPAD row selected for D.
  scenario.expected.first_read_address = 7;
  return scenario;
}

} // namespace tb
} // namespace smesh
