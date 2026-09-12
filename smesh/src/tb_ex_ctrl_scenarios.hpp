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

  // Standalone PRELOAD feeds D from the final source row back to the first.
  std::array<std::uint32_t, kDim> preload_read_addresses{};

  // Two standalone COMPUTEs feed A and B forward from their respective bases.
  std::array<std::uint32_t, 2 * kDim> compute_a_read_addresses{};
  std::array<std::uint32_t, 2 * kDim> compute_b_read_addresses{};
  std::array<MeshAccumRow, 2 * kDim> compute_output_rows{};
};

// One test program plus the observations expected from that program.
struct ExCtrlScenario {
  // A scenario is both the command stream presented as if it came from RS
  // and the observations we expect while that stream moves through ExCtrl.
  const char* name = "";
  std::array<SmeshIssue, 4> program{};
  ExCtrlExpected expected{};
};

// Build one RS-like command with a tag, opcode, and packed operands.
inline SmeshIssue makeScenarioIssue(SmeshRsTag tag, SmeshFunct funct,
                                    std::uint64_t rs1 = 0, std::uint64_t rs2 = 0,
                                    bool tag_valid = true) {
  SmeshIssue issue{};
  issue.rs_tag_valid = bit(tag_valid);
  issue.rs_tag = tag;
  issue.cmd.funct = static_cast<std::uint32_t>(funct);
  issue.cmd.rs1 = rs1;
  issue.cmd.rs2 = rs2;
  return issue;
}

// Create a CONFIG -> PRELOAD -> COMPUTE_FLIP -> COMPUTE_STAY scenario.
inline ExCtrlScenario makeConfigPreloadComputeScenario() {
  ExCtrlScenario scenario{};
  scenario.name = "config_preload_compute_flip_stay";

  // The test deliberately uses a short, readable command sequence:
  //   1. CONFIG_EX: establish WS mode and unit address strides.
  //   2. PRELOAD:   read weights from SPAD row 4 and target C at ACC row 8.
  //   3. FLIP:      read A from SPAD row 12 and addends from SPAD row 4.
  //   4. STAY:      reuse the weights with A at row 8 and addends at row 0.
  // Each command has a distinct RS tag so queue movement is easy to inspect.
  scenario.program = {
      makeScenarioIssue(7, SmeshFunct::Config,      packConfigExRs1(1),                 packConfigExRs2(1)),
      makeScenarioIssue(8, SmeshFunct::Preload,     packLocal(makeSpAddr(4),  {kDim, kDim}), packLocal(makeAccAddr(8), {kDim, kDim})),
      makeScenarioIssue(9, SmeshFunct::ComputeFlip, packLocal(makeSpAddr(12), {kDim, kDim}), packLocal(makeSpAddr(4),  {kDim, kDim})),
      makeScenarioIssue(10, SmeshFunct::ComputeStay, packLocal(makeSpAddr(8), {kDim, kDim}), packLocal(makeSpAddr(0),  {kDim, kDim})),
  };

  // The first row-address view is taken from the COMPUTE command. D is fed in
  // reverse row order, so its first address is row 4 + (DIM - 1) = 7.
  scenario.expected.rowaddr_a_address = 12;
  scenario.expected.rowaddr_b_address = 4;
  scenario.expected.rowaddr_d_address = 7;

  // The first real read request is the bank-local SPAD row selected for D.
  scenario.expected.first_read_address = 7;
  scenario.expected.preload_read_addresses = {7, 6, 5, 4};
  scenario.expected.compute_a_read_addresses = {12, 13, 14, 15, 8, 9, 10, 11};
  scenario.expected.compute_b_read_addresses = {4, 5, 6, 7, 0, 1, 2, 3};

  for (std::size_t row = 0; row < 2 * kDim; ++row) {
    for (std::size_t col = 0; col < kDim; ++col) {
      Acc value = static_cast<Acc>(scenario.expected.compute_b_read_addresses[row] * kDim + col);
      for (std::size_t k = 0; k < kDim; ++k) {
        const Acc a = static_cast<Acc>(scenario.expected.compute_a_read_addresses[row] * kDim + k);
        const Acc b = static_cast<Acc>((4 + k) * kDim + col);
        value += a * b;
      }
      scenario.expected.compute_output_rows[row][col] = value;
    }
  }
  return scenario;
}

} // namespace tb
} // namespace smesh
