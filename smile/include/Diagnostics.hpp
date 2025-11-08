// **********************************************************************
// smile/include/Diagnostics.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 2 2025
/*
For diagnosing tiles that don't finish their program with expected exit() call.
*/
#pragma once
#include "Tile1.hpp" // for MemoryPort

namespace smile {

void verify_and_report_postmortem(
    Tile1& tile,
    MemoryPort& mem_port,
    const ThreadContext threads[2],
    const bool saw_breakpoint_trap[2],
    const bool saw_ecall_trap[2],
    const uint32_t breakpoint_mepc[2],
    const uint32_t ecall_mepc[2],
    int cycle);
} // namespace smile
