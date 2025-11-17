// **********************************************************************
// smile/src/tb_tile1.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 6 2025
/*
Testbench for a RV tile.
*/

#include <descore/Parameter.hpp>

#include "Tile1.hpp"
#include "Debugger.hpp"
#include "Diagnostics.hpp"
#include "util/FlatBinLoader.hpp"
#include "../../smicro/src/Dram.hpp"
#include "AccelPort.hpp"

#include <cascade/Clock.hpp>
#include <cascade/SimDefs.hpp>
#include <cascade/SimGlobals.hpp>

#include <string>
#include <iostream>
#include <iomanip>

// **************
// Parameters (CLI flags): name, default value, help text
// **************
BoolParameter(showcontexts, false, "List component instance names (contexts) and exit");
StringParameter(prog, "", "Path to flat binary file (.bin) to load");
IntParameter(load_addr, 0x0, "Physical load address for the flat binary");
IntParameter(start_pc, 0x0, "Initial PC (set core's PC before run)");
IntParameter(steps, 0, "Cycles to auto-run; <=0 enters interactive debugger");
BoolParameter(ignore_bpfile, false,
  "Do not load .smile_dbg breakpoint file on startup");

// Simple adapter/shim class that exposes smicro's Dram as a MemoryPort for Tile1.
class DramMemoryPort : public MemoryPort {
public:
  explicit DramMemoryPort(Dram& dram) : dram_(dram) {}

  uint32_t read32(uint32_t addr) override {
    uint32_t value = 0;
    const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr);
    dram_.read(phys, &value, sizeof(value));
    return value;
  }

  void write32(uint32_t addr, uint32_t value) override {
    const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr);
    dram_.write(phys, &value, sizeof(value));
  }

private:
  Dram& dram_;
};

/* A basic accelerator on issue it logs and returns rs1 + rs2 */
class LoggingAccelPort : public AccelPort {
public:
  void issue(uint32_t raw_inst,
             uint32_t pc,
             uint32_t rs1_val,
             uint32_t rs2_val) override {
    std::ios_base::fmtflags old_flags = std::cout.flags();
    char old_fill = std::cout.fill('0');

    std::cout << "[ACCEL] pc=0x"
              << std::hex << std::setw(8) << pc
              << " inst=0x" << std::setw(8) << raw_inst
              << " rs1=0x" << std::setw(8) << rs1_val
              << " rs2=0x" << std::setw(8) << rs2_val
              << std::dec << std::endl;

    std::cout.fill(old_fill);
    std::cout.flags(old_flags);

    resp_ = rs1_val + rs2_val;
    has_resp_ = true;
  }

  bool has_response() const override {
    return has_resp_;
  }

  uint32_t read_response() override {
    has_resp_ = false;
    return resp_;
  }

private:
  bool has_resp_ = false;
  uint32_t resp_ = 0;
};

int main(int argc, char* argv[]) {
  // **************
  // Step 1: Parse tracing, parameters, and dump options
  // **************
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  // **************
  // Step 2: Create components
  // **************
  Tile1 tile("tile1");
  Dram dram("dram", 0);
  DramMemoryPort dram_port(dram);
  LoggingAccelPort accel_port;
  tile.attach_memory(&dram_port);
  tile.attach_accelerator(&accel_port); // attach accelerator (attach_accelerator in Tile1.hpp)
  dram.s_req.wireToZero();
  dram.s_resp.sendToBitBucket();

  // **************
  // Step 3: Optional: list component instance names & exit
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
  std::string prog_path = std::string(prog);
  const bool using_default_program = prog_path.empty();
  if (!prog_path.empty()) {
    uint32_t nbytes = 0;
    bool ok = load_flat_bin(prog_path, &dram_port, static_cast<uint32_t>(load_addr), &nbytes);
    assert_always(ok, "Program load failed");
    if (static_cast<uint32_t>(start_pc) != 0u) {
      tile.set_pc(static_cast<uint32_t>(start_pc));
    }
  } else {
    const uint32_t program[] = {
      0x00500093u,
      0x00308113u,
      0x002081B3u,
      0x00000073u
    };
    uint32_t addr = static_cast<uint32_t>(load_addr);
    for (size_t i = 0; i < sizeof(program) / sizeof(program[0]); ++i, addr += 4) {
      dram_port.write32(addr, program[i]);
    }
    if (static_cast<uint32_t>(start_pc) != 0u) {
      tile.set_pc(static_cast<uint32_t>(start_pc));
    }
  }
  (void)using_default_program;

  // **************
  // Step 6: Run simulation
  // **************
  int max_cycles = static_cast<int>(steps);
  smile::DebuggerState dbg(tile, dram_port);
  if (max_cycles > 0) {
    smile::auto_run(dbg, max_cycles);
  } else {
    smile::run_debugger(dbg, ignore_bpfile);
  }

  // **************
  // Step 7A: Sim stop on exit() via ecall 93
  // **************
  if (dbg.program_exited) {
    for (const auto& ctx : dbg.threads) {
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
