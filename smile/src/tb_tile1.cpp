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
#include "AccelArraySum.hpp"

#include <cascade/Clock.hpp>
#include <cascade/SimDefs.hpp>
#include <cascade/SimGlobals.hpp>

#include <cstdio>
#include <string>

// **************
// Parameters (CLI flags): name, default value, help text
// **************
BoolParameter(showcontexts, false, "List component instance names (contexts) and exit");
StringParameter(prog, "", "Path to flat binary file (.bin) to load");
IntParameter(load_addr, 0x0, "Physical load address for the flat binary");
IntParameter(start_pc, 0x0, "Initial PC (set core's PC before run)");
IntParameter(steps, 0, "Cycles to auto-run; <=0 enters interactive debugger");
IntParameter(sw_threads, 1, "Software thread contexts to schedule (1 or 2). Default: 1");
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

  // not doing anything yet
  // but will ultimately be used to simulate memory latency more realistically 
  // by enqueuing requests and returning responses after some cycles
  void cycle() override {}

  bool can_request() const override {
    return !resp_valid_;
  }

  void request_read32(uint32_t addr) override {
    resp_data_ = read32(addr);
    resp_valid_ = true;
  }

  void request_write32(uint32_t addr, uint32_t value) override {
    write32(addr, value);
    resp_data_ = 0;
    resp_valid_ = true;
  }

  bool resp_valid() const override {
    return resp_valid_;
  }

  uint32_t resp_data() const override {
    return resp_data_;
  }

  void resp_consume() override {
    resp_valid_ = false; // allow new requests after consuming response
  }

private:
  Dram& dram_;
  bool resp_valid_ = false; // for more realistic memory port behavior
  uint32_t resp_data_ = 0;  // for more realistic memory port behavior
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
  AccelArraySum accel_port(dram_port);  // array-sum accelerator, backed by dram_port for mem ops
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
    // const uint32_t program[] = {
    //   0x00500093u, // addi x1, x0, 5
    //   0x00308113u, // addi x2, x1, 3
    //   0x002081B3u, // add  x3, x1, x2
    //   0x05D00893u, // addi x17(a7), x0, 93  -> ecall exit syscall
    //   0x00000513u, // addi x10(a0), x0, 0   -> exit code 0
    //   0x00000073u  // ecall
    // };  
    // Preload a small test array in DRAM at 0x100
    const uint32_t array_base = 0x00000100u;
    const uint32_t array[] = {
      1u,
      2u,
      3u,
      4u
    }; // sum = 10
    uint32_t addr = array_base;
    for (size_t i = 0; i < sizeof(array) / sizeof(array[0]); ++i, addr += 4) {
      dram_port.write32(addr, array[i]);
    }
    // Program that calls AccelArraySum via CUSTOM-0:
    //   rs1 = base (0x100), rs2 = len (4), rd = x3
    const uint32_t program[] = {
      0x10000093u, // addi x1, x0, 256     ; x1 = 0x00000100 (array_base)
      0x00400113u, // addi x2, x0, 4       ; x2 = 4 (length in words)
      0x0020818bu, // custom0 x3, x1, x2   ; x3 = sum(arr[0..3]) = 10
      0x00018533u, // add   x10, x3, x0    ; a0 = x3 (exit code = sum)
      0x05D00893u, // addi  x17, x0, 93    ; a7 = 93 (exit syscall)
      0x00000073u  // ecall                ; exit(a0)
    };
    // uint32_t addr = static_cast<uint32_t>(load_addr);
    addr = static_cast<uint32_t>(load_addr);
    for (size_t i = 0; i < sizeof(program) / sizeof(program[0]); ++i, addr += 4) {
      dram_port.write32(addr, program[i]); // write program to dram_port
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
  int num_threads = static_cast<int>(sw_threads); // we pass tread count to debugger state
  if (num_threads < 1) num_threads = 1;
  if (num_threads > 2) num_threads = 2;
  smile::DebuggerState dbg(tile, dram_port, num_threads); 
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
    printf("[EXIT] Program exited with code %u\n", tile.exit_code());
    printf("[STATS] inst=%llu alu=%llu add=%llu mul=%llu loads=%llu stores=%llu branches=%llu taken=%llu\n",
           (unsigned long long)tile.inst_count(),
           (unsigned long long)tile.arith_count(),
           (unsigned long long)tile.add_count(),
           (unsigned long long)tile.mul_count(),
           (unsigned long long)tile.load_count(),
           (unsigned long long)tile.store_count(),
           (unsigned long long)tile.branch_count(),
           (unsigned long long)tile.branch_taken_count());
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
