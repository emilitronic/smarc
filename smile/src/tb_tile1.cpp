// **********************************************************************
// smile/src/tb_tile1.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 6 2025
/*
Testbench for a RV tile.
*/

#include <descore/Parameter.hpp>
#include "Tile1.hpp"
#include "Diagnostics.hpp"
#include "util/FlatBinLoader.hpp"
#include "../../smicro/src/Dram.hpp"

#include <cascade/Clock.hpp>
#include <cascade/SimDefs.hpp>
#include <cascade/SimGlobals.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// **************
// Parameters (CLI flags): name, default value, help text
// **************
BoolParameter(showcontexts, false, "List component instance names (contexts) and exit");
StringParameter(prog, "", "Path to flat binary file (.bin) to load");
IntParameter(load_addr, 0x0, "Physical load address for the flat binary");
IntParameter(start_pc, 0x0, "Initial PC (set core's PC before run)");
IntParameter(steps, 0, "Cycles to auto-run; <=0 enters interactive debugger");

// Simple adapter/shim class that exposes smicro's Dram as a MemoryPort for Tile1.
// i.e., convert Tile's interface to SoC's Dram definition
class DramMemoryPort : public MemoryPort
{ // inherit
public:
  explicit DramMemoryPort(Dram &dram) : dram_(dram) {} // construct (use ref Dram obj as input), init member dram_ with dram
  uint32_t read32(uint32_t addr) override
  { // read 32b value at stored addr
    uint32_t value = 0;
    const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr); // DRAM's base addr + tile-relative addr
    dram_.read(phys, &value, sizeof(value));
    return value;
  }
  void write32(uint32_t addr, uint32_t value) override
  { // write 32b value to store addr
    const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr);
    dram_.write(phys, &value, sizeof(value));
  }

private:
  Dram &dram_;
};

namespace
{

  /* holds persistent debugger state (threads, traps, exit bookkeeping) */
  struct DebuggerState {
    Tile1 &tile; // reference to Tile1 being debugged
    DramMemoryPort &dram_port;
    ThreadContext threads[2];
    bool saw_breakpoint_trap[2];
    bool saw_ecall_trap[2];
    uint32_t breakpoint_mepc[2];
    uint32_t ecall_mepc[2];
    uint32_t last_breakpoint_log_mepc[2];
    bool program_exited = false;
    uint32_t program_exit_code = 0;
    bool user_quit = false;
    int current_thread = 1;
    int cycle = 0;
    bool trace_enabled = false;
    std::vector<uint32_t> breakpoints;

    DebuggerState(Tile1 &t, DramMemoryPort &d) : tile(t), dram_port(d) // constructor
    {
      reset();
    }

    void reset()
    {
      for (int thread = 0; thread < 2; ++thread)
      {
        threads[thread].active = true;
        threads[thread].pc = tile.pc();
        for (int r = 0; r < 32; ++r)
        {
          threads[thread].regs[r] = 0;
        }
        threads[thread].regs[0] = 0;
        saw_breakpoint_trap[thread] = false;
        saw_ecall_trap[thread] = false;
        breakpoint_mepc[thread] = 0;
        ecall_mepc[thread] = 0;
        last_breakpoint_log_mepc[thread] = 0xFFFFFFFFu; // init breakpoint log pc to invalid
      }
      program_exited = false;
      program_exit_code = 0;
      user_quit = false;
      current_thread = 1;
      cycle = 0;
      trace_enabled = false;
      breakpoints.clear();
    }
  };

  /* per-cycle execution metadata returned by execute_cycle() */
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

  /* check whether any thread context is still marked active */
  static bool has_active_threads(const DebuggerState &state) {
    return state.threads[0].active || state.threads[1].active;
  }

  /* convert to lower case */
  static std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return value;
  }

  /* attempt to parse user text into uint32_t, reject malformed input */
  static bool parse_u32(const std::string &text, uint32_t *value) {
    try
    {
      size_t idx = 0;
      const unsigned long parsed = std::stoul(text, &idx, 0);
      if (idx != text.size() || parsed > std::numeric_limits<uint32_t>::max())
      {
        return false;
      }
      *value = static_cast<uint32_t>(parsed);
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  /* helper to generate 8-digit hex strings */
  static std::string hex32(uint32_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << value;
    return oss.str();
  }

  /* verbose dump of PC, cause, regs, and memory when ebreak fires */
  static void print_breakpoint_snapshot(DebuggerState &state, int thread_index, uint32_t pc, uint32_t mcause) {
    std::ios_base::fmtflags old_flags = std::cout.flags();
    char old_fill = std::cout.fill('0');

    std::cout << "[T" << thread_index << "] breakpoint pc=0x"
              << std::hex << std::setw(8) << pc
              << " mcause=0x" << std::setw(8) << mcause
              << " mstatus=0x" << std::setw(8) << state.tile.mstatus()
              << std::dec << std::endl;

    std::cout << "  regs:";
    for (int reg = 1; reg <= 7; ++reg)
    {
      std::cout << " x" << reg << "=0x"
                << std::hex << std::setw(8) << state.threads[thread_index].regs[reg]
                << std::dec;
    }
    std::cout << " a4=0x"
              << std::hex << std::setw(8) << state.threads[thread_index].regs[14]
              << std::dec;

    std::cout << std::endl
              << "  mem:";
    for (uint32_t addr = 0x0100u; addr <= 0x0110u; addr += 4u)
    {
      const uint32_t val = state.dram_port.read32(addr);
      std::cout << " [0x" << std::hex << std::setw(8) << addr
                << "]=0x" << std::setw(8) << val
                << std::dec;
    }

    std::cout << std::endl;
    std::cout.fill(old_fill);
    std::cout.flags(old_flags);
  }

  /* lightweight trace line describing current cycle, thread, and instruction */
  static void print_cycle_trace(const DebuggerState &state, const CycleInfo &info) {
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

  /* dump all architectural registers for both threads in hex */
  static void print_registers(const DebuggerState &state) {
    std::ios_base::fmtflags old_flags = std::cout.flags();
    char old_fill = std::cout.fill('0');

    for (int t = 0; t < 2; ++t)
    {
      std::cout << "[T" << t << "] pc=0x"
                << std::hex << std::setw(8) << state.threads[t].pc
                << std::dec << " active=" << (state.threads[t].active ? "yes" : "no")
                << std::endl;
      for (int r = 0; r < 32; ++r)
      {
        std::cout << "  x" << std::setw(2) << r << "=0x"
                  << std::hex << std::setw(8) << state.threads[t].regs[r]
                  << std::dec;
        if ((r % 4) == 3)
        {
          std::cout << std::endl;
        }
        else
        {
          std::cout << ' ';
        }
      }
      std::cout << std::endl;
    }

    std::cout.fill(old_fill);
  std::cout.flags(old_flags);
}

/* dump registers for one specified thread */
static void print_registers_for_thread(const DebuggerState &state, int t) {
  if (t < 0 || t > 1) {
    std::cout << "Invalid thread index (expected 0 or 1)" << std::endl;
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

/* dump a single register of a thread (plus context) */
static void print_single_register(const DebuggerState &state, int t, int r) {
  if (t < 0 || t > 1) {
    std::cout << "Invalid thread index (expected 0 or 1)" << std::endl;
    return;
  }
  if (r < 0 || r >= 32) {
    std::cout << "Invalid register index (expected 0-31)" << std::endl;
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

/* raw memory viewer that prints N consecutive words */
static void dump_memory(DramMemoryPort &dram_port, uint32_t addr, std::size_t count){
    std::ios_base::fmtflags old_flags = std::cout.flags();
    char old_fill = std::cout.fill('0');

    for (std::size_t i = 0; i < count; ++i)
    {
      const uint32_t current = addr + static_cast<uint32_t>(i * 4u);
      const uint32_t value = dram_port.read32(current);
      std::cout << "  [0x" << std::hex << std::setw(8) << current
                << "] = 0x" << std::setw(8) << value
                << std::dec << std::endl;
    }

    std::cout.fill(old_fill);
    std::cout.flags(old_flags);
  }

  /* execute 1 cycle, record trap metadata */
  static CycleInfo execute_cycle(DebuggerState &state, bool honor_breakpoints) {
    // ---- init bookkeeping ----
    CycleInfo info;
    if (!has_active_threads(state)) {
      return info;
    }
    // ---- round-robin scheduling logic ----
    int attempts = 0;
    do {
      state.current_thread = (state.current_thread + 1) & 1;
      ++attempts;
      if (state.threads[state.current_thread].active) {
        break;
      }
    } while (attempts < 2);
    // ---- if selected thread inactive, nothing to do ----
    if (!state.threads[state.current_thread].active) {
      return info;
    }

    ThreadContext &context = state.threads[state.current_thread]; // reference to selected thread context (load it below)

    // ---- check for user breakpoints before executing cycle ----
    if (honor_breakpoints) {
      // check if current PC matches any user breakpoint, if so stop and hand control back to debugger
      if (std::find(state.breakpoints.begin(), state.breakpoints.end(), context.pc) != state.breakpoints.end())
      {
        info.thread = state.current_thread;
        info.begin_pc = context.pc;
        info.instruction = state.dram_port.read32(context.pc);
        info.user_breakpoint_hit = true;
        info.mcause = state.tile.mcause();
        return info;
      }
    }

    // ---- execute 1 cycle for the selected thread ----
    const uint32_t begin_pc = context.pc;
    state.tile.load_context(context);       // load thread context into Tile1 hardware
    Sim::run(); // <--------------------------- EXECUTE 1 CYCLE
    state.tile.save_context(context);
    state.cycle++;

    info.executed = true;
    info.thread = state.current_thread;
    info.begin_pc = begin_pc;
    info.instruction = state.dram_port.read32(begin_pc);
    info.mcause = state.tile.mcause();

    // ---- if your program exited normally (syscall 93) ----
    if (state.tile.has_exited()) {
      if (!state.program_exited) {
        state.program_exit_code = state.tile.exit_code();
        std::cout << "Program exited with code " << state.program_exit_code << std::endl;
      }
      state.program_exited = true;
      state.threads[0].active = false;
      state.threads[1].active = false;
      info.program_exited = true;
      return info;
    }

    // ---- trap bookkeeping: if your program did not exit normally or we saw breakpoint ----
    const bool executed_breakpoint = info.instruction == 0x00100073u; // did we just execute ebreak?
    info.executed_breakpoint_instr = executed_breakpoint;
    // avoid duplicate logging of same breakpoint
    if (executed_breakpoint && begin_pc != state.last_breakpoint_log_mepc[state.current_thread]) {
      info.log_breakpoint_snapshot = true;
      state.last_breakpoint_log_mepc[state.current_thread] = begin_pc;
    }
    // record trap metadata and resume past the ebreak
    if (executed_breakpoint) {
      if (!state.saw_breakpoint_trap[state.current_thread]) {
        state.saw_breakpoint_trap[state.current_thread] = true;
        state.breakpoint_mepc[state.current_thread] = begin_pc;
      }
      state.threads[state.current_thread].pc = begin_pc + 4u;
    }
    // detect ecall (environment calls) traps
    if (!state.saw_ecall_trap[state.current_thread] &&
        info.mcause == static_cast<uint32_t>(Tile1::TrapCause::EnvironmentCallFromMMode))
    {
      state.saw_ecall_trap[state.current_thread] = true;
      state.ecall_mepc[state.current_thread] = state.tile.mepc();
    }

    return info;
  }

  /* execute cycles until all threads halted/exited, optionally print snapshot on ebreak */
  static void auto_run(DebuggerState &state, int max_cycles) {
    for (int i = 0; i < max_cycles; ++i) {
      if (!has_active_threads(state)) {
        break;
      }
      CycleInfo info = execute_cycle(state, false); // run cycle without honoring debugger breakpoints
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

  /* interactive command loop for stepping, tracing, and breakpoints */
  static void run_debugger(DebuggerState &state) {
    std::cout << "Entering Tile1 debugger. Type 'help' for commands." << std::endl;
    std::string line;

    while (true) {
      std::cout << "smile> " << std::flush; // put i/p into line
      if (!std::getline(std::cin, line)) {
        state.user_quit = true;
        break;
      }

      std::istringstream iss(line);
      std::string command;
      iss >> command; // get 1st word of i/p (i.e., the command)
      if (command.empty()) {
        continue;
      }

      const std::string cmd = to_lower(command); // cnvrt to lower case
      if (cmd == "step") {
        uint32_t count = 1;
        std::string count_token;
        if (iss >> count_token)
        { // get 2nd word of i/p (possibly a step count)
          if (!parse_u32(count_token, &count) || count == 0) {
            std::cout << "Invalid step count" << std::endl;
            continue;
          }
        }
        for (uint32_t i = 0; i < count; ++i) {
          CycleInfo info = execute_cycle(state, false); // run cycle as user requested
          if (!info.executed) {
            if (!has_active_threads(state)) {
              std::cout << "No active threads remain." << std::endl;
            }
            break;
          }
          print_cycle_trace(state, info); // always print basic trace for instr
          if (info.log_breakpoint_snapshot)
          { // print detailed context view on 1st ebreak
            print_breakpoint_snapshot(state, info.thread, info.begin_pc, info.mcause);
          }
          if (info.executed_breakpoint_instr)
          { // pause on ebreak
            std::cout << "Software breakpoint executed at 0x"
                      << hex32(info.begin_pc) << std::endl;
            break;
          }
          if (info.program_exited)
          { // detect clean program exit via ecall 93
            break;
          }
        }
      }
      else if (cmd == "cont" || cmd == "continue") {
        while (has_active_threads(state)) {
          CycleInfo info = execute_cycle(state, true);
          if (!info.executed) {
            if (info.user_breakpoint_hit) {
              std::cout << "Hit breakpoint at 0x"
                        << hex32(info.begin_pc) << std::endl;
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
            std::cout << "Software breakpoint executed at 0x"
                      << hex32(info.begin_pc) << std::endl;
            break;
          }
          if (info.program_exited) {
            break;
          }
        }
      }
      else if (cmd == "break" || cmd == "br") {
        std::string addr_token;
        if (!(iss >> addr_token)) {
          if (state.breakpoints.empty()) {
            std::cout << "No breakpoints set" << std::endl;
          }
          else {
            std::cout << "Breakpoints:" << std::endl;
            for (uint32_t addr : state.breakpoints) {
              std::cout << "  0x" << hex32(addr) << std::endl;
            }
          }
          continue;
        }
        uint32_t addr = 0;
        if (!parse_u32(addr_token, &addr)) {
          std::cout << "Invalid address" << std::endl;
          continue;
        }
        if (std::find(state.breakpoints.begin(), state.breakpoints.end(), addr) == state.breakpoints.end()) {
          state.breakpoints.push_back(addr);
          std::cout << "Breakpoint added at 0x"
                    << hex32(addr) << std::endl;
        }
        else {
          std::cout << "Breakpoint already exists at 0x"
                    << hex32(addr) << std::endl;
        }
      }
      else if (cmd == "delete" || cmd == "del") {
        std::string addr_token;
        if (!(iss >> addr_token)) {
          std::cout << "Usage: delete <addr>" << std::endl;
          continue;
        }
        uint32_t addr = 0;
        if (!parse_u32(addr_token, &addr)) {
          std::cout << "Invalid address" << std::endl;
          continue;
        }
        auto it = std::find(state.breakpoints.begin(), state.breakpoints.end(), addr);
        if (it != state.breakpoints.end()) {
          state.breakpoints.erase(it);
          std::cout << "Breakpoint removed at 0x" << hex32(addr) << std::endl;
        }
        else {
          std::cout << "No breakpoint at 0x" << hex32(addr) << std::endl;
        }
      }
      else if (cmd == "clear") {
        if (!state.breakpoints.empty()) {
          state.breakpoints.clear();
        }
        std::cout << "All breakpoints cleared" << std::endl;
      }
      else if (cmd == "regs") {                        
        std::string token;
        if (!(iss >> token)) {               // dump all threads & registers
          print_registers(state);
        }
        else {                         
          const std::size_t colon = token.find(':');
          if (colon == std::string::npos) {  // dump all registers for one thread
            uint32_t thread_idx = 0;
            if (!parse_u32(token, &thread_idx)) {
              std::cout << "Invalid thread index" << std::endl;
              continue;
            }
            print_registers_for_thread(state, static_cast<int>(thread_idx));
          }
          else {                             // dump one register for one thread
            const std::string t_str   = token.substr(0, colon);
            const std::string reg_str = token.substr(colon + 1);
            uint32_t thread_idx = 0;
            uint32_t reg_idx = 0;
            if (!parse_u32(t_str, &thread_idx)) {
              std::cout << "Invalid thread index" << std::endl;
              continue;
            }
            if (!parse_u32(reg_str, &reg_idx)) {
              std::cout << "Invalid register index" << std::endl;
              continue;
            }
            print_single_register(state, static_cast<int>(thread_idx),
                                  static_cast<int>(reg_idx));
          }
        }
      }
      else if (cmd == "mem") {
        std::string addr_token;
        if (!(iss >> addr_token)) {
          std::cout << "Usage: mem <addr> [count]" << std::endl;
          continue;
        }
        uint32_t addr = 0;
        if (!parse_u32(addr_token, &addr)) {
          std::cout << "Invalid address" << std::endl;
          continue;
        }
        std::string count_token;
        std::size_t count = 4;
        if (iss >> count_token) {
          uint32_t parsed = 0;
          if (!parse_u32(count_token, &parsed)) {
            std::cout << "Invalid count" << std::endl;
            continue;
          }
          count = static_cast<std::size_t>(parsed);
        }
        if (count == 0) {
          std::cout << "Count must be greater than zero" << std::endl;
          continue;
        }
        dump_memory(state.dram_port, addr, count);
      }
      else if (cmd == "trace") {
        std::string mode;
        if (iss >> mode) {
          mode = to_lower(mode);
          if (mode == "on") {
            state.trace_enabled = true;
          }
          else if (mode == "off") {
            state.trace_enabled = false;
          }
          else {
            std::cout << "Usage: trace [on|off]" << std::endl;
            continue;
          }
        }
        else {
          state.trace_enabled = !state.trace_enabled;
        }
        std::cout << "Trace " << (state.trace_enabled ? "enabled" : "disabled") << std::endl;
      }
      else if (cmd == "quit" || cmd == "q") {
        state.user_quit = true;
        break;
      }
      else if (cmd == "help") {
        std::cout << "Commands:\n"
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
      }
      else {
        std::cout << "Unknown command: " << command << std::endl;
      }

      if (state.program_exited) {
        break;
      }
    }
  }

} // namespace

int main(int argc, char *argv[])
{
  // **************
  // Step 1: Parse tracing, parameters, and dump options
  // **************
  descore::parseTraces(argc, argv);        // scans argv for trace options
  Parameter::parseCommandLine(argc, argv); // parses cmd line flags and fills *Parameter() globals (above)
  Sim::parseDumps(argc, argv);             // dump signals, denote what to write to VCD waves

  // **************
  // Step 2: Create components
  // **************
  Tile1 tile("tile1");
  Dram dram("dram", 0);
  DramMemoryPort dram_port(dram);
  tile.attach_memory(&dram_port); // connect tile to memory
  dram.s_req.wireToZero();        // neutralize unused DRAM FIFO ports for this...
  dram.s_resp.sendToBitBucket();  // ...standalone harness to prevent sim warnings

  // **************
  // Step 3: Optional: list component instance names & exit (contruct & register components first)
  // **************
  if (showcontexts) {
    Sim::dumpComponentNames();
    return 0;
  }

  // **************
  // Step 4: Hook clock and initialize & reset simulator
  // **************
  Clock clk;
  tile.clk << clk;
  dram.clk << clk;
  clk.generateClock();
  Sim::init();
  Sim::reset();

  // **************
  // Step 5: Load program (a flat .bin file)
  // **************
  std::string prog_path = std::string(prog); // ensure prog_path is a regular C++ string
  const bool using_default_program = prog_path.empty();
  if (!prog_path.empty()) // if find prog_path specified on cmd line
  {
    uint32_t nbytes = 0;
    bool ok = load_flat_bin(prog_path, &dram_port, (uint32_t)load_addr, &nbytes);
    assert_always(ok, "Program load failed");
    if ((uint32_t)start_pc != 0u)
    {
      tile.set_pc((uint32_t)start_pc);
    }
  }
  else // else load this (x1<-5; x2<-x1+3; x3<-x1+x2; ecall)
  {
    const uint32_t program[] = {
        0x00500093u,
        0x00308113u,
        0x002081B3u,
        0x00000073u};
    uint32_t addr = (uint32_t)load_addr;
    for (size_t i = 0; i < sizeof(program) / sizeof(program[0]); ++i, addr += 4) // load instrs into mem
    {
      dram_port.write32(addr, program[i]);
    }
    if ((uint32_t)start_pc != 0u) // if user specified start_pc, use it
    {
      tile.set_pc((uint32_t)start_pc);
    }
  }
  (void)using_default_program;

  // **************
  // Step 6: Run simulation
  // **************
  int max_cycles = static_cast<int>(steps);
  DebuggerState dbg(tile, dram_port); // init debugger state
  if (max_cycles > 0) {
    auto_run(dbg, max_cycles);
  }
  else {
    run_debugger(dbg);
  }

  // **************
  // Step 7A: Sim stop on exit() via ecall 93
  // **************
  if (dbg.program_exited){
    for (const auto &ctx : dbg.threads) {
      assert_always(ctx.regs[0] == 0, "x0 must remain zero");
    }
    return 0;
  }

  if (dbg.user_quit) {
    return 0;
  }

  // **************
  // Step 7B: Sim stop NOT on exit(): post-mortem sanity check
  // **************
  smile::verify_and_report_postmortem(tile, dram_port, dbg.threads,
                                      dbg.saw_breakpoint_trap, dbg.saw_ecall_trap,
                                      dbg.breakpoint_mepc, dbg.ecall_mepc,
                                      dbg.cycle);

  return 0;
}
