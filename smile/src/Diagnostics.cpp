// **********************************************************************
// smile/src/Diagnostics.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 2 2025
/*
For diagnosing tiles that don't finish their program with expected exit() call.
*/
#include "Diagnostics.hpp"
#include <iostream>
#include <iomanip>

namespace smile {

constexpr uint32_t ECALL_FLAG_ADDR      = 0x0104u;
constexpr uint32_t BREAKPOINT_FLAG_ADDR = 0x0108u;
constexpr uint32_t ECALL_FLAG_VALUE     = 0xDEADu;
constexpr uint32_t BREAKPOINT_FLAG_VALUE= 0xBEEFu;

void verify_and_report_postmortem(
    Tile1& tile,
    MemoryPort& mem_port,
    const ThreadContext threads[2],
    const bool saw_breakpoint_trap[2],
    const bool saw_ecall_trap[2],
    const uint32_t breakpoint_mepc[2],
    const uint32_t ecall_mepc[2],
    int cycle)
{
  // === 1) Trap coverage ===
  // if no thread performed exit(), check if at least 1 thread hit ebreak trap or executed an ecall
  const bool any_breakpoint_trap = saw_breakpoint_trap[0] || saw_breakpoint_trap[1];
  const bool any_ecall_trap      = saw_ecall_trap[0]      || saw_ecall_trap[1];
  assert_always(any_breakpoint_trap, "Breakpoint trap was not observed");
  assert_always(any_ecall_trap,      "ECALL trap was not observed");

  // === 2) Alignment checks ===
  // ensure trap handlers stored sensible, aligned values for both threads
  for (int t = 0; t < 2; ++t) {
    if (saw_breakpoint_trap[t])
      assert_always((breakpoint_mepc[t] & 0x3u) == 0u, "Breakpoint mepc misaligned");
    if (saw_ecall_trap[t])
      assert_always((ecall_mepc[t] & 0x3u) == 0u, "ECALL mepc misaligned");
  }

  // === 3) Memory flag checks ===
  // ensure trap handler correctly wrote my program's magic values in correct spots
  const uint32_t breakpoint_flag = mem_port.read32(BREAKPOINT_FLAG_ADDR);
  const uint32_t ecall_flag      = mem_port.read32(ECALL_FLAG_ADDR);
  assert_always(breakpoint_flag == BREAKPOINT_FLAG_VALUE,
                "Breakpoint trap did not set flag at 0x0108");
  assert_always(ecall_flag == ECALL_FLAG_VALUE,
                "ECALL trap did not set flag at 0x0104");

  // === 4) Status + invariants ===
  // a) trap handler should have saved its previous privilege mode in the MPP field of mstatus as M (confirming correct trap entry)
  // b) x0 is still zero at the end — another core invariant  
  const uint32_t mstatus = tile.mstatus();
  assert_always((mstatus & Tile1::MSTATUS_MPP_MASK) == Tile1::MSTATUS_MPP_MACHINE,
                "mstatus.MPP expected to hold previous mode (Machine) inside handler");
  assert_always(tile.reg(0) == 0, "x0 must remain zero");

  // === 5) Summary ===
  // total cycle count, final trap flags in memory, which threads triggered ebreak or ecall, final vals of key CSRs
  std::cout << "Cycle count: " << (cycle + 1)
            << " breakpoint flag=0x" << std::hex << breakpoint_flag
            << " ecall flag=0x" << ecall_flag << std::dec << std::endl;

  std::cout << "Trap summary:";
  for (int t = 0; t < 2; ++t) {
    if (saw_breakpoint_trap[t])
      std::cout << " [T" << t << "] breakpoint mepc=0x"
                << std::hex << breakpoint_mepc[t] << std::dec;
    if (saw_ecall_trap[t])
      std::cout << " [T" << t << "] ecall mepc=0x"
                << std::hex << ecall_mepc[t] << std::dec;
  }
  std::cout << " mcause=0x" << std::hex << tile.mcause()
            << " mstatus=0x" << mstatus << std::dec << std::endl;
}

} // namespace smile
