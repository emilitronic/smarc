// **********************************************************************
// smile/include/Debugger.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 16 2025
/*
Debugger REPLfor Tile1 simulator.
*/
#pragma once

#include "Tile1.hpp"

#include <cstdint>
#include <vector>

namespace smile {

struct DebuggerState {
  Tile1 &tile;
  MemoryPort &mem;
  int configured_threads; // let debugger know if 1 or 2 threads are configured
  ThreadContext threads[2];
  bool saw_breakpoint_trap[2];
  bool saw_ecall_trap[2];
  uint32_t breakpoint_mepc[2];
  uint32_t ecall_mepc[2];
  uint32_t last_breakpoint_log_mepc[2];
  bool program_exited;
  uint32_t program_exit_code;
  bool user_quit;
  int current_thread;
  int cycle;
  bool trace_enabled;
  std::vector<uint32_t> breakpoints;

  DebuggerState(Tile1 &t, MemoryPort &m, int thread_count);
  void reset();
};

void auto_run(DebuggerState &state, int max_cycles);
void run_debugger(DebuggerState &state, bool ignore_bpfile); // accept ignore_bpfile CLI flag

} // namespace smile
