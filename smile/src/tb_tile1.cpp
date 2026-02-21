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
#include "MemCtrlTimedPort.hpp"
#include "../../smicro/src/Dram.hpp"
#include "AccelPort.hpp"
#include "AccelArraySum.hpp"
#include "AccelArraySumMc.hpp"
#include "AccelDemoAdd.hpp"

#include <cascade/Clock.hpp>
#include <cascade/SimDefs.hpp>
#include <cascade/SimGlobals.hpp>

#include <cstdio>
#include <string>
#include <algorithm>
#include <cctype>
#include <memory>
#include <vector>

// **************
// Parameters (CLI flags): name, default value, help text
// **************
BoolParameter(showcontexts, false, "List component instance names (contexts) and exit");
StringParameter(prog, "", "Path to flat binary file (.bin) to load");
IntParameter(load_addr, 0x0, "Physical load address for the flat binary");
IntParameter(start_pc, 0x0, "Initial PC (set core's PC before run)");
IntParameter(mem_latency, 0, "Fixed memory latency (cycles) for MemCtrlTimedPort");
BoolParameter(ideal_mem, false, "Use ideal memory model in Tile1 (sync read32/write32, no stalls)");
StringParameter(mem_model, "timed", "Tile1 memory model: timed|ideal");
StringParameter(accel, "array_sum", "Accelerator: none|demo_add|array_sum|array_sum_mc");
StringParameter(suite, "proto_accel_sum", "Built-in suite when -prog is empty: proto_accel_sum|proto_accel_sum_altaddr|proto_accel_sum_badarg|proto_accel_sum_unsupported|proto_accel_sum_twice");
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
  MemCtrlTimedPort memctrl(&dram_port, (int)mem_latency);
  tile.attach_memory(&memctrl);
  // Configure accelerator based on accel parameter (none/demo_add/array_sum/array_sum_mc)
  std::unique_ptr<AccelPort> accel_ptr;
  std::string accel_flag = std::string(accel);
  std::transform(accel_flag.begin(), accel_flag.end(), accel_flag.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); // lowercase accel flag
  if (accel_flag == "none") {
    accel_ptr.reset();
  } else if (accel_flag == "demo_add") {
    accel_ptr = std::make_unique<AccelDemoAdd>(memctrl);
  } else if (accel_flag == "array_sum") {
    accel_ptr = std::make_unique<AccelArraySum>(memctrl);
  } else if (accel_flag == "array_sum_mc") {
    accel_ptr = std::make_unique<AccelArraySumMc>(memctrl);
  } else {
    assert_always(false, "accel must be 'none', 'demo_add', 'array_sum', or 'array_sum_mc'");
  }
  tile.attach_accelerator(accel_ptr.get());
  
  std::string mem_model_flag = std::string(mem_model);
  std::transform(mem_model_flag.begin(), mem_model_flag.end(), mem_model_flag.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (ideal_mem || mem_model_flag == "ideal") {
    tile.set_mem_model(Tile1::MemModel::Ideal);
  } else {
    assert_always(mem_model_flag == "timed", "mem_model must be 'timed' or 'ideal'");
    tile.set_mem_model(Tile1::MemModel::Timed);
  }
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
  bool suite_active = false;
  bool suite_twice  = false;
  std::string suite_name;
  uint32_t suite_expected_exit = 0;
  uint32_t suite_expected_sum  = 0;
  if (!prog_path.empty()) {
    uint32_t nbytes = 0;
    bool ok = load_flat_bin(prog_path, &dram_port, static_cast<uint32_t>(load_addr), &nbytes);
    assert_always(ok, "Program load failed");
    if (static_cast<uint32_t>(start_pc) != 0u) {
      tile.set_pc(static_cast<uint32_t>(start_pc));
    }
  } else { // if no program specified, inject a simple test program based on the suite parameter
    auto encode_lui = [](uint32_t rd, uint32_t imm20) -> uint32_t {
      return ((imm20 & 0xfffffu) << 12) | ((rd & 0x1fu) << 7) | 0x37u;
    };
    auto encode_addi = [](uint32_t rd, uint32_t rs1, int32_t imm12) -> uint32_t {
      const uint32_t imm = static_cast<uint32_t>(imm12) & 0xfffu;
      return (imm << 20) | ((rs1 & 0x1fu) << 15) | (0x0u << 12) | ((rd & 0x1fu) << 7) | 0x13u;
    };
    auto encode_sw = [](uint32_t rs2, uint32_t rs1, int32_t imm12) -> uint32_t {
      const uint32_t imm = static_cast<uint32_t>(imm12) & 0xfffu;
      const uint32_t imm_lo = (imm & 0x1fu) << 7;
      const uint32_t imm_hi = ((imm >> 5) & 0x7fu) << 25;
      return imm_hi | ((rs2 & 0x1fu) << 20) | ((rs1 & 0x1fu) << 15) | (0x2u << 12) | imm_lo | 0x23u;
    };
    auto encode_ecall = []() -> uint32_t {
      return 0x00000073u;
    };
    auto encode_custom0 = [](uint32_t rd, uint32_t rs1, uint32_t rs2, uint32_t funct3) -> uint32_t {
      return (0x00u << 25) | ((rs2 & 0x1fu) << 20) | ((rs1 & 0x1fu) << 15) |
             ((funct3 & 0x7u) << 12) | ((rd & 0x1fu) << 7) | 0x0bu;
    };
    auto emit_li = [&](std::vector<uint32_t>& out, uint32_t rd, uint32_t value) {
      const uint32_t hi = (value + 0x800u) >> 12;
      const int32_t lo = static_cast<int32_t>(value) - static_cast<int32_t>(hi << 12);
      assert_always(lo >= -2048 && lo <= 2047, "suite li immediate out of range");
      out.push_back(encode_lui(rd, hi));
      out.push_back(encode_addi(rd, rd, lo));
    };

    std::string suite_flag = std::string(suite);
    std::transform(suite_flag.begin(), suite_flag.end(), suite_flag.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    suite_name = suite_flag;
    suite_active = true;

    uint32_t array_addr = 0x00000100u; // preserve historical default behavior for proto_accel_sum
    uint32_t init_base = array_addr;
    uint32_t len_words = 4u;
    uint32_t funct3 = 0u;

    if (suite_flag == "proto_accel_sum") {
      // preserved default behavior: base=0x100, len=4, expect sum=10
    } else if (suite_flag == "proto_accel_sum_altaddr") {
      array_addr = 0x00006000u;
      init_base = array_addr;
      len_words = 16u;
    } else if (suite_flag == "proto_accel_sum_badarg") {
      array_addr = 0x00004002u; // pass misaligned base to accelerator
      init_base = 0x00004000u;  // initialize backing data at aligned address
      len_words = 16u;
    } else if (suite_flag == "proto_accel_sum_unsupported") {
      array_addr = 0x00004000u;
      init_base = array_addr;
      len_words = 16u;
      funct3 = 1u; // unsupported verb id
    } else if (suite_flag == "proto_accel_sum_twice") {
      array_addr = 0x00004000u;
      init_base = array_addr;
      len_words = 16u;
      suite_twice = true;
    } else {
      assert_always(false, "unknown suite for injected program path");
    }

    uint32_t expected_sum = 0;
    for (uint32_t i = 0; i < len_words; ++i) {
      const uint32_t value = i + 1u;
      expected_sum += value;
      dram_port.write32(init_base + 4u * i, value);
    }
    suite_expected_sum = expected_sum;

    const uint32_t mailbox0 = 0x00000100u;
    const uint32_t mailbox1 = 0x00000104u;
    if (suite_twice) {
      dram_port.write32(mailbox0, 0u);
      dram_port.write32(mailbox1, 0u);
    }

    std::vector<uint32_t> program;
    program.reserve(suite_twice ? 24u : 16u);
    if (suite_twice) {
      emit_li(program, 1u, mailbox0);                       // x1 = mailbox base
      emit_li(program, 2u, array_addr);                     // x2 = array base
      emit_li(program, 4u, len_words);                      // x4 = len
      program.push_back(encode_custom0(3u, 2u, 4u, 0u));    // x3 = sum
      program.push_back(encode_sw(3u, 1u, 0));              // sw x3, 0(x1)
      program.push_back(encode_custom0(3u, 2u, 4u, 0u));    // x3 = sum again
      program.push_back(encode_sw(3u, 1u, 4));              // sw x3, 4(x1)
      program.push_back(encode_addi(10u, 0u, 0));           // a0 = exit code 0
    } else {
      emit_li(program, 2u, array_addr);                     // x2 = array base
      emit_li(program, 4u, len_words);                      // x4 = len
      program.push_back(encode_custom0(3u, 2u, 4u, funct3));// x3 = accel result
      program.push_back(encode_addi(10u, 3u, 0));           // a0 = x3
    }
    program.push_back(encode_addi(17u, 0u, 93));            // a7 = 93
    program.push_back(encode_ecall());                      // ecall exit

    uint32_t addr = static_cast<uint32_t>(load_addr);
    for (size_t i = 0; i < program.size(); ++i, addr += 4u) {
      dram_port.write32(addr, program[i]);
    }

    if (static_cast<uint32_t>(start_pc) != 0u) {
      tile.set_pc(static_cast<uint32_t>(start_pc));
    } else {
      tile.set_pc(static_cast<uint32_t>(load_addr));
    }

    if (suite_twice) {
      suite_expected_exit = 0u;
    } else if (suite_flag == "proto_accel_sum_badarg") {
      suite_expected_exit = AccelPort::ACCEL_E_BADARG;
    } else if (suite_flag == "proto_accel_sum_unsupported") {
      suite_expected_exit = AccelPort::ACCEL_E_UNSUPPORTED;
    } else {
      suite_expected_exit = expected_sum;
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
    if (suite_active) {
      if (suite_twice) {
        const uint32_t got0 = dram_port.read32(0x00000100u);
        const uint32_t got1 = dram_port.read32(0x00000104u);
        assert_always(got0 == suite_expected_sum, "suite twice mailbox0 mismatch");
        assert_always(got1 == suite_expected_sum, "suite twice mailbox1 mismatch");
        assert_always(tile.exit_code() == suite_expected_exit, "suite twice exit code mismatch");
        printf("[SUITE] %s PASS mailbox0=0x%x mailbox1=0x%x expected=0x%x\n",
               suite_name.c_str(), got0, got1, suite_expected_sum);
      } else {
        const uint32_t got = tile.exit_code();
        assert_always(got == suite_expected_exit, "suite exit code mismatch");
        printf("[SUITE] %s PASS expected=0x%x got=0x%x\n",
               suite_name.c_str(), suite_expected_exit, got);
      }
    }
    for (const auto& ctx : dbg.threads) {
      assert_always(ctx.regs[0] == 0, "x0 must remain zero");
    }
    printf("[EXIT] Program exited with code %u\n", tile.exit_code());
    printf("[STATS] cycles=%llu inst=%llu alu=%llu add=%llu mul=%llu loads=%llu stores=%llu branches=%llu taken=%llu\n",
           (unsigned long long)dbg.cycle,
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
  // Step 7B (non-exit): Print stats even if the program did not call exit(93)
  // (e.g., smurf stops via breakpoint/trap and is validated by postmortem checks).
  // **************
  printf("[STATS] cycles=%llu inst=%llu alu=%llu add=%llu mul=%llu loads=%llu stores=%llu branches=%llu taken=%llu\n",
         (unsigned long long)dbg.cycle,
         (unsigned long long)tile.inst_count(),
         (unsigned long long)tile.arith_count(),
         (unsigned long long)tile.add_count(),
         (unsigned long long)tile.mul_count(),
         (unsigned long long)tile.load_count(),
         (unsigned long long)tile.store_count(),
         (unsigned long long)tile.branch_count(),
         (unsigned long long)tile.branch_taken_count());

  // **************
  // Step 7C: Sim stop NOT on exit(): post-mortem sanity check
  // **************
  smile::verify_and_report_postmortem(tile, dram_port, dbg.threads,
    dbg.saw_breakpoint_trap, dbg.saw_ecall_trap,
    dbg.breakpoint_mepc, dbg.ecall_mepc,
    dbg.cycle);

  return 0;
}
