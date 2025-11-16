// **********************************************************************
// smile/src/util/Debugger.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 16 2025 

#include "Debugger.hpp"

#include <cascade/SimGlobals.hpp>

#include <algorithm>
#include <cctype>
#include <fstream> // for breakpoint persistence
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace smile {
namespace {

struct CycleInfo {
  int thread = -1;
  uint32_t begin_pc = 0;
  uint32_t instruction = 0;
  uint32_t mcause = 0;
  bool executed = false;
  bool executed_breakpoint_instr = false;
  bool log_breakpoint_snapshot = false;
  bool user_breakpoint_hit = false;
  bool program_exited = false;
};

static constexpr const char* COLOR_RESET = "\033[0m";
static constexpr const char* COLOR_BP    = "\033[33m";
static constexpr const char* COLOR_EXIT  = "\033[32m";
static constexpr const char* COLOR_ERR   = "\033[31m";
static constexpr const char* COLOR_HINT  = "\033[36m";
static constexpr const char* BREAKPOINT_FILE = ".smile_dbg";

static bool has_active_threads(const DebuggerState& state) {
  return state.threads[0].active || state.threads[1].active;
}

static std::string to_lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

static bool parse_u32(const std::string& text, uint32_t* value) {
  try {
    size_t idx = 0;
    const unsigned long parsed = std::stoul(text, &idx, 0);
    if (idx != text.size() || parsed > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    *value = static_cast<uint32_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

static std::string hex32(uint32_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setw(8) << std::setfill('0') << value;
  return oss.str();
}

/* breakpoint persistence: writes current set after every add/delete/clear/quit */
static void save_breakpoints_to_file(const DebuggerState& state) {
  std::ofstream out(BREAKPOINT_FILE, std::ios::trunc);
  if (!out) {
    std::cout << COLOR_ERR
              << "Warning: unable to write breakpoint file "
              << BREAKPOINT_FILE
              << COLOR_RESET << std::endl;
    return;
  }

  for (uint32_t addr : state.breakpoints) {
    out << "0x" << hex32(addr) << "\n";
  }
}

/* repopulates state.breakpoints on REPL entry (w/ cyan status line) */
static void load_breakpoints_from_file(DebuggerState& state) {
  std::ifstream in(BREAKPOINT_FILE);
  if (!in) {
    return;
  }

  state.breakpoints.clear();
  std::string line;
  while (std::getline(in, line)) {
    std::string token;
    std::istringstream iss(line);
    if (!(iss >> token)) {
      continue;
    }
    uint32_t addr = 0;
    if (parse_u32(token, &addr)) {
      if (std::find(state.breakpoints.begin(), state.breakpoints.end(), addr) == state.breakpoints.end()) {
        state.breakpoints.push_back(addr);
      }
    }
  }

  if (!state.breakpoints.empty()) {
    std::cout << COLOR_HINT
              << "Loaded " << state.breakpoints.size()
              << " breakpoint(s) from " << BREAKPOINT_FILE
              << COLOR_RESET << std::endl;
  }
}

static void print_breakpoint_snapshot(DebuggerState& state, int thread_index,
                                      uint32_t pc, uint32_t mcause) {
  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  std::cout << COLOR_BP
            << "[BP][T" << thread_index << "] breakpoint pc=0x"
            << std::hex << std::setw(8) << pc
            << " mcause=0x" << std::setw(8) << mcause
            << " mstatus=0x" << std::setw(8) << state.tile.mstatus()
            << std::dec << COLOR_RESET << std::endl;

  std::cout << "  regs:";
  for (int reg = 1; reg <= 7; ++reg) {
    std::cout << " x" << reg << "=0x"
              << std::hex << std::setw(8) << state.threads[thread_index].regs[reg]
              << std::dec;
  }
  std::cout << " a4=0x"
            << std::hex << std::setw(8) << state.threads[thread_index].regs[14]
            << std::dec;

  std::cout << std::endl << "  mem:";
  for (uint32_t addr = 0x0100u; addr <= 0x0110u; addr += 4u) {
    const uint32_t val = state.mem.read32(addr);
    std::cout << " [0x" << std::hex << std::setw(8) << addr
              << "]=0x" << std::setw(8) << val
              << std::dec;
  }

  std::cout << std::endl;
  std::cout.fill(old_fill);
  std::cout.flags(old_flags);
}

static void print_cycle_trace(const DebuggerState& state, const CycleInfo& info) {
  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  std::cout << "cycle " << state.cycle
            << " [T" << info.thread << "] pc=0x"
            << std::hex << std::setw(8) << info.begin_pc
            << " instr=0x" << std::setw(8) << info.instruction
            << std::dec << std::endl;

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);
}

static void print_registers(const DebuggerState& state) {
  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  for (int t = 0; t < 2; ++t) {
    std::cout << "[T" << t << "] pc=0x"
              << std::hex << std::setw(8) << state.threads[t].pc
              << std::dec << " active=" << (state.threads[t].active ? "yes" : "no")
              << std::endl;
    for (int r = 0; r < 32; ++r) {
      std::cout << "  x" << std::setw(2) << r << "=0x"
                << std::hex << std::setw(8) << state.threads[t].regs[r]
                << std::dec;
      if ((r % 4) == 3) {
        std::cout << std::endl;
      } else {
        std::cout << ' ';
      }
    }
    std::cout << std::endl;
  }

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);
}

static void print_registers_for_thread(const DebuggerState& state, int t) {
  if (t < 0 || t > 1) {
    std::cout << COLOR_ERR << "Invalid thread index (expected 0 or 1)" << COLOR_RESET << std::endl;
    return;
  }

  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  std::cout << "[T" << t << "] pc=0x"
            << std::hex << std::setw(8) << state.threads[t].pc
            << std::dec << " active=" << (state.threads[t].active ? "yes" : "no")
            << std::endl;
  for (int r = 0; r < 32; ++r) {
    std::cout << "  x" << std::setw(2) << r << "=0x"
              << std::hex << std::setw(8) << state.threads[t].regs[r]
              << std::dec;
    if ((r % 4) == 3) {
      std::cout << std::endl;
    } else {
      std::cout << ' ';
    }
  }
  std::cout << std::endl;

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);
}

static void print_single_register(const DebuggerState& state, int t, int r) {
  if (t < 0 || t > 1) {
    std::cout << COLOR_ERR << "Invalid thread index (expected 0 or 1)" << COLOR_RESET << std::endl;
    return;
  }
  if (r < 0 || r >= 32) {
    std::cout << COLOR_ERR << "Invalid register index (expected 0-31)" << COLOR_RESET << std::endl;
    return;
  }

  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  std::cout << "[T" << t << "] x" << r
            << "=0x" << std::hex << std::setw(8) << state.threads[t].regs[r]
            << std::dec
            << " (pc=0x" << std::hex << std::setw(8) << state.threads[t].pc
            << std::dec << " active=" << (state.threads[t].active ? "yes" : "no")
            << ")"
            << std::endl;

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);
}

static void dump_memory(MemoryPort& mem, uint32_t addr, std::size_t count) {
  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  for (std::size_t i = 0; i < count; ++i) {
    const uint32_t current = addr + static_cast<uint32_t>(i * 4u);
    const uint32_t value = mem.read32(current);
    std::cout << "  [0x" << std::hex << std::setw(8) << current
              << "] = 0x" << std::setw(8) << value
              << std::dec << std::endl;
  }

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);
}

static CycleInfo execute_cycle(DebuggerState& state, bool honor_breakpoints) {
  CycleInfo info;
  if (!has_active_threads(state)) {
    return info;
  }

  int attempts = 0;
  do {
    state.current_thread = (state.current_thread + 1) & 1;
    ++attempts;
    if (state.threads[state.current_thread].active) {
      break;
    }
  } while (attempts < 2);

  if (!state.threads[state.current_thread].active) {
    return info;
  }

  ThreadContext& context = state.threads[state.current_thread];

  if (honor_breakpoints) {
    if (std::find(state.breakpoints.begin(), state.breakpoints.end(), context.pc) != state.breakpoints.end()) {
      info.thread = state.current_thread;
      info.begin_pc = context.pc;
      info.instruction = state.mem.read32(context.pc);
      info.user_breakpoint_hit = true;
      info.mcause = state.tile.mcause();
      return info;
    }
  }

  const uint32_t begin_pc = context.pc;
  state.tile.load_context(context);
  Sim::run();
  state.tile.save_context(context);
  state.cycle++;

  info.executed = true;
  info.thread = state.current_thread;
  info.begin_pc = begin_pc;
  info.instruction = state.mem.read32(begin_pc);
  info.mcause = state.tile.mcause();

  if (state.tile.has_exited()) {
    if (!state.program_exited) {
      state.program_exit_code = state.tile.exit_code();
      std::cout << COLOR_EXIT
                << "[EXIT] Program exited with code "
                << state.program_exit_code
                << COLOR_RESET << std::endl;
    }
    state.program_exited = true;
    state.threads[0].active = false;
    state.threads[1].active = false;
    info.program_exited = true;
    return info;
  }

  const bool executed_breakpoint = info.instruction == 0x00100073u;
  info.executed_breakpoint_instr = executed_breakpoint;
  if (executed_breakpoint && begin_pc != state.last_breakpoint_log_mepc[state.current_thread]) {
    info.log_breakpoint_snapshot = true;
    state.last_breakpoint_log_mepc[state.current_thread] = begin_pc;
  }
  if (executed_breakpoint) {
    if (!state.saw_breakpoint_trap[state.current_thread]) {
      state.saw_breakpoint_trap[state.current_thread] = true;
      state.breakpoint_mepc[state.current_thread] = begin_pc;
    }
    state.threads[state.current_thread].pc = begin_pc + 4u;
  }
  if (!state.saw_ecall_trap[state.current_thread] &&
      info.mcause == static_cast<uint32_t>(Tile1::TrapCause::EnvironmentCallFromMMode)) {
    state.saw_ecall_trap[state.current_thread] = true;
    state.ecall_mepc[state.current_thread] = state.tile.mepc();
  }

  return info;
}

} // namespace

DebuggerState::DebuggerState(Tile1& t, MemoryPort& m)
  : tile(t), mem(m) {
  reset();
}

void DebuggerState::reset() {
  for (int thread = 0; thread < 2; ++thread) {
    threads[thread].active = true;
    threads[thread].pc = tile.pc();
    for (int r = 0; r < 32; ++r) {
      threads[thread].regs[r] = 0;
    }
    threads[thread].regs[0] = 0;
    saw_breakpoint_trap[thread] = false;
    saw_ecall_trap[thread] = false;
    breakpoint_mepc[thread] = 0;
    ecall_mepc[thread] = 0;
    last_breakpoint_log_mepc[thread] = 0xFFFFFFFFu;
  }
  program_exited = false;
  program_exit_code = 0;
  user_quit = false;
  current_thread = 1;
  cycle = 0;
  trace_enabled = false;
  breakpoints.clear();
}

void auto_run(DebuggerState& state, int max_cycles) {
  for (int i = 0; i < max_cycles; ++i) {
    if (!has_active_threads(state)) {
      break;
    }
    CycleInfo info = execute_cycle(state, false);
    if (!info.executed) {
      break;
    }
    if (info.log_breakpoint_snapshot) {
      print_breakpoint_snapshot(state, info.thread, info.begin_pc, info.mcause);
    }
    if (info.program_exited) {
      break;
    }
  }
}

void run_debugger(DebuggerState& state) {
  load_breakpoints_from_file(state); // e.g. .smile_dbg 
  std::cout << "Entering Tile1 debugger. Type 'help' for commands." << std::endl;
  std::string line;

  while (true) {
    std::cout << "smile> " << std::flush;
    if (!std::getline(std::cin, line)) {
      state.user_quit = true;
      break;
    }

    std::istringstream iss(line);
    std::string command;
    iss >> command;
    if (command.empty()) {
      continue;
    }

    const std::string cmd = to_lower(command);
    if (cmd == "step") {
      uint32_t count = 1;
      std::string count_token;
      if (iss >> count_token) {
        if (!parse_u32(count_token, &count) || count == 0) {
          std::cout << COLOR_ERR << "Invalid step count" << COLOR_RESET << std::endl;
          continue;
        }
      }
      for (uint32_t i = 0; i < count; ++i) {
        CycleInfo info = execute_cycle(state, false);
        if (!info.executed) {
          if (!has_active_threads(state)) {
            std::cout << "No active threads remain." << std::endl;
          }
          break;
        }
        print_cycle_trace(state, info);
        if (info.log_breakpoint_snapshot) {
          print_breakpoint_snapshot(state, info.thread, info.begin_pc, info.mcause);
        }
        if (info.executed_breakpoint_instr) {
          std::cout << COLOR_BP
                    << "[BP] Software breakpoint executed at 0x"
                    << hex32(info.begin_pc)
                    << COLOR_RESET << std::endl;
          break;
        }
        if (info.program_exited) {
          break;
        }
      }
    } else if (cmd == "cont" || cmd == "continue") {
      while (has_active_threads(state)) {
        CycleInfo info = execute_cycle(state, true);
        if (!info.executed) {
          if (info.user_breakpoint_hit) {
            std::cout << COLOR_BP
                      << "[BP] Hit breakpoint at 0x"
                      << hex32(info.begin_pc)
                      << COLOR_RESET << std::endl;
            print_breakpoint_snapshot(state, info.thread, info.begin_pc, info.mcause);
          }
          break;
        }
        if (state.trace_enabled) {
          print_cycle_trace(state, info);
        }
        if (info.log_breakpoint_snapshot) {
          print_breakpoint_snapshot(state, info.thread, info.begin_pc, info.mcause);
          break;
        }
        if (info.executed_breakpoint_instr) {
          std::cout << COLOR_BP
                    << "[BP] Software breakpoint executed at 0x"
                    << hex32(info.begin_pc)
                    << COLOR_RESET << std::endl;
          break;
        }
        if (info.program_exited) {
          break;
        }
      }
    } else if (cmd == "break" || cmd == "br") {
      std::string addr_token;
      if (!(iss >> addr_token)) {
        if (state.breakpoints.empty()) {
          std::cout << "No breakpoints set" << std::endl;
        } else {
          std::cout << "Breakpoints:" << std::endl;
          for (uint32_t addr : state.breakpoints) {
            std::cout << "  0x" << hex32(addr) << std::endl;
          }
        }
        continue;
      }
      uint32_t addr = 0;
      if (!parse_u32(addr_token, &addr)) {
        std::cout << COLOR_ERR << "Invalid address" << COLOR_RESET << std::endl;
        continue;
      }
      if (std::find(state.breakpoints.begin(), state.breakpoints.end(), addr) == state.breakpoints.end()) {
        state.breakpoints.push_back(addr);
        save_breakpoints_to_file(state);
        std::cout << "Breakpoint added at 0x"
                  << hex32(addr) << std::endl;
      } else {
        std::cout << "Breakpoint already exists at 0x"
                  << hex32(addr) << std::endl;
      }
    } else if (cmd == "delete" || cmd == "del") {
      std::string addr_token;
      if (!(iss >> addr_token)) {
        std::cout << "Usage: delete <addr>" << std::endl;
        continue;
      }
      uint32_t addr = 0;
      if (!parse_u32(addr_token, &addr)) {
        std::cout << COLOR_ERR << "Invalid address" << COLOR_RESET << std::endl;
        continue;
      }
      auto it = std::find(state.breakpoints.begin(), state.breakpoints.end(), addr);
      if (it != state.breakpoints.end()) {
        state.breakpoints.erase(it);
        save_breakpoints_to_file(state);
        std::cout << "Breakpoint removed at 0x" << hex32(addr) << std::endl;
      } else {
        std::cout << "No breakpoint at 0x" << hex32(addr) << std::endl;
      }
    } else if (cmd == "clear") {
      if (!state.breakpoints.empty()) {
        state.breakpoints.clear();
        save_breakpoints_to_file(state);
      }
      std::cout << "All breakpoints cleared" << std::endl;
    } else if (cmd == "regs") {
      std::string token;
      if (!(iss >> token)) {
        print_registers(state);
      } else {
        const std::size_t colon = token.find(':');
        if (colon == std::string::npos) {
          uint32_t thread_idx = 0;
          if (!parse_u32(token, &thread_idx)) {
            std::cout << COLOR_ERR << "Invalid thread index" << COLOR_RESET << std::endl;
            continue;
          }
          print_registers_for_thread(state, static_cast<int>(thread_idx));
        } else {
          const std::string t_str = token.substr(0, colon);
          const std::string reg_str = token.substr(colon + 1);
          uint32_t thread_idx = 0;
          uint32_t reg_idx = 0;
          if (!parse_u32(t_str, &thread_idx)) {
            std::cout << COLOR_ERR << "Invalid thread index" << COLOR_RESET << std::endl;
            continue;
          }
          if (!parse_u32(reg_str, &reg_idx)) {
            std::cout << COLOR_ERR << "Invalid register index" << COLOR_RESET << std::endl;
            continue;
          }
          print_single_register(state, static_cast<int>(thread_idx),
                                static_cast<int>(reg_idx));
        }
      }
    } else if (cmd == "mem") {
      std::string addr_token;
      if (!(iss >> addr_token)) {
        std::cout << "Usage: mem <addr> [count]" << std::endl;
        continue;
      }
      uint32_t addr = 0;
      if (!parse_u32(addr_token, &addr)) {
        std::cout << COLOR_ERR << "Invalid address" << COLOR_RESET << std::endl;
        continue;
      }
      std::string count_token;
      std::size_t count = 4;
      if (iss >> count_token) {
        uint32_t parsed = 0;
        if (!parse_u32(count_token, &parsed)) {
          std::cout << COLOR_ERR << "Invalid count" << COLOR_RESET << std::endl;
          continue;
        }
        count = static_cast<std::size_t>(parsed);
      }
      if (count == 0) {
        std::cout << "Count must be greater than zero" << std::endl;
        continue;
      }
      dump_memory(state.mem, addr, count);
    } else if (cmd == "trace") {
      std::string mode;
      if (iss >> mode) {
        mode = to_lower(mode);
        if (mode == "on") {
          state.trace_enabled = true;
        } else if (mode == "off") {
          state.trace_enabled = false;
        } else {
          std::cout << "Usage: trace [on|off]" << std::endl;
          continue;
        }
      } else {
        state.trace_enabled = !state.trace_enabled;
      }
      std::cout << "Trace " << (state.trace_enabled ? "enabled" : "disabled") << std::endl;
    } else if (cmd == "quit" || cmd == "q") {
      state.user_quit = true;
      break;
    } else if (cmd == "help") {
      std::cout << COLOR_HINT << "Commands:" << COLOR_RESET << "\n"
                << "  step [N]           - advance N cycles (default 1)\n"
                << "  cont               - run until breakpoint or exit\n"
                << "  break <addr>       - set breakpoint at PC address\n"
                << "  delete <addr>      - remove breakpoint at PC address\n"
                << "  clear              - remove all breakpoints\n"
                << "  regs               - dump all registers for both threads\n"
                << "  regs <t>           - dump registers for thread t (0 or 1)\n"
                << "  regs <t>:<reg>     - dump register x<reg> for thread t\n"
                << "  mem <addr> [count] - dump memory words\n"
                << "  trace [on|off]     - toggle per-cycle tracing\n"
                << "  quit               - exit debugger\n";
    } else {
      std::cout << "Unknown command: " << command << std::endl;
    }

    if (state.program_exited) {
      break;
    }
  }

  save_breakpoints_to_file(state);
}

} // namespace smile
