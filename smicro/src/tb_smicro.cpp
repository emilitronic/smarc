// **********************************************************************
// smicro/src/tb_smicro.cpp
// **********************************************************************
// S Magierowski Aug 16 2025
/*
SoC test harness with a single-switch suite to avoid ambiguity.
Suites: 
hal_* run only DRAM HAL at t=0 (no driver traffic). 
proto_* run protocol timing via core or tester.
some details
proto_accel_sum_altaddr: same test as proto_accel_sum but with alt addr mapping to verify addr translation in accelerator memory bridge (AccelMemBridge) and its interaction with DRAM base address.
proto_accel_sum_badarg: sets array_addr = 0x4002 (not 4-byte aligned) and verifies return to mailbox of error code ACCEL_E_BADARG

to configure, build, and run:
cedar % cmake -S . -B build  -DCEDAR_DIR=/Users/seb/Research/Cascade/cedar -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cedar % cmake --build build --target smicro -j
cedar/build/smicro % ./smicro <options>
*/
#if 0
// **********************************************************************
// High-level Flow (Guideposts)
// **********************************************************************
// Step 1: Parse CLI (-trace, -driver/-suite/-test, -mem_latency, etc.).
// Step 2: Construct SoC and choose traffic source (core vs tester) based on -driver.
// Step 3: Optional early-exit: -showcontexts prints component instance names.
// Step 4: Configure timing/policy (MemCtrl latency, posted writes).
// Step 5: Hook up a single clock and initialize the simulator.
// Step 6: Print a short banner with effective settings.
// Step 7: Run Layer-1 DRAM HAL test at t=0 (content/bounds only).
// Step 8: Run Layer-2 protocol/timing suite over cycles (requires -driver=test).
// Step 9: Execute either a batch run (-steps=N) or an interactive loop (Enter to step).
//         Optional fence (-drain or 'f') advances until posted stores drain.
// **********************************************************************
#endif
#include <descore/Parameter.hpp>
#include <cstddef> // proto_accel_sum needs size_t for vector params
#include <cstdint> // for uint32_t, etc. in proto_accel_sum
#include <iostream>
#include <vector>  // for vector parameters in proto_accel_sum
#include "SoC.hpp"
#include "AccelCmd.hpp" 

using namespace std;

// **************
// Parameters (CLI flags): name, default value, help text
// **************
StringParameter(topo,       "via_l2", "Topology: via_l1|via_l2|dram|priv"); // defaults topo is via_l2
IntParameter(steps,          0,      "Batch steps; 0=interactive");
// New single-switch suite
StringParameter(suite,      "proto_core", "Suite: hal_none|hal_multi|hal_bounds|proto_core|proto_accel_sum|proto_accel_sum_altaddr|proto_accel_sum_badarg|proto_raw|proto_no_raw|proto_rar|proto_lat");
IntParameter(mem_latency,     3, "MemCtrl latency (cycles)");
IntParameter(dram_latency,   -1, "[deprecated] use -mem_latency; if >=0 overrides mem_latency");
BoolParameter(drain,         false, "After run, fence: keep stepping until posted stores drain");
BoolParameter(showcontexts,  false, "List component instance names (contexts) and exit");
BoolParameter(posted_writes, true, "Enable posted write ACKs (1=posted, 0=ack on drain)");

static AttachMode parse_mode(const std::string& topo) {
  if (topo == "via_l1") return ViaL1;
  if (topo == "via_l2") return ViaL2;
  if (topo == "dram")   return ToDRAM;
  if (topo == "priv")   return PrivateDRAM;
  return ViaL2;
}
// helper to translate CPU address to DRAM physical address (DRAM base + offset)
static inline uint64_t cpu_to_phys(const SoC& soc, uint32_t cpu_addr) {
  assert_always(soc.dram_ != nullptr, "cpu_to_phys: missing DRAM");
  return soc.dram_->get_base() + static_cast<uint64_t>(cpu_addr);
}

int main (int argc, char *argv[]) {
  // **************
  // Step 1: Parse tracing, parameters, and dump options
  // **************
  descore::parseTraces(argc, argv);        // scans argv for trace options
  Parameter::parseCommandLine(argc, argv); // parses cmd line flags and fills *Parameter() globals (above)
  Sim::parseDumps(argc, argv);             // dump signals, denote what to write to VCD waves

  // **************
  // Step 2: Resolve suite and select traffic source; then build SoC
  // **************
  std::string S = std::string(suite); // assign suite settings to variable S
  bool is_hal   = S.rfind("hal_",   0) == 0;         // search backward up to index 0; if matches return 0
  bool is_proto = S.rfind("proto_", 0) == 0;
  assert_always(is_hal || is_proto, "unknown -suite"); // descore assertion that's never compiled-out, if cond fails prints message & aborts 
  bool use_tester = is_proto && (S != "proto_core") && (S != "proto_accel_sum") && (S != "proto_accel_sum_altaddr") && (S != "proto_accel_sum_badarg"); // tester for proto_* except core-driven suites
  SoC soc(parse_mode(topo), use_tester);             // invoke SoC object in desired config
  
  // **************
  // Step 3: Optional: list component instance names and exit
  // **************
  if (showcontexts) { Sim::dumpComponentNames(); return 0; }
  
  // **************
  // Step 4: Configure timing/policy
  // Choose effective latency: deprecated -dram_latency overrides when provided (>=0)
  // **************
  int eff_lat = (dram_latency >= 0) ? (int)dram_latency : (int)mem_latency;
  soc.set_mem_latency(eff_lat);
  soc.set_posted_writes(posted_writes);
  
  // **************
  // Step 5: Hook clock and initialize simulator
  // **************
  Clock clk;
  soc.clk << clk;
  clk.generateClock();
  Sim::init();
  
  // **************
  // Step 6: Banner (what will run)
  // **************
  // Print banner before running any internal suite cycles so it appears at the top
  cout << "Press return to advance a clock cycle" << endl;
  cout << "Press 0 to reset" << endl;
  cout << "Press \"q\" to quit" << endl;
  cout << "Tip: set MemCtrl latency with -mem_latency=N (alias: -dram_latency=N)" << endl;
  cout << "MemCtrl latency (cycles): " << eff_lat << endl;
  cout << "MemCtrl posted writes: " << (posted_writes ? "on" : "off") << endl;
  cout << "Suite: " << S << endl;
  if (is_hal) cout << "Driver: none" << endl; else cout << "Driver: " << (use_tester ? "tester" : "core") << endl;

  // **************
  // Step 7: HAL — DRAM tests (t=0 only; bypass MemCtrl timing)
  // **************
  // Simple table-driven tests via DRAM HAL (content/bounds only)
  auto run_hal = [/* nothing to capture from surrounding scope */](const std::string& S, SoC& soc) -> bool {
    auto* d = soc.dram_;
    if (!d) return false;
    if (S == "hal_none") return true;
    if (S == "hal_multi") {
      // Write 3 8-B vals to DRAM & read back & make sure they are correct
      uint64_t A=d->get_base()+0x4000, B=A+8, C=B+8, r=0, x=0x11, y=0x22, z=0x33;
      d->write(A,&x,8);
      d->write(B,&y,8);
      d->write(C,&z,8);
      d->read(A,&r,8);  assert_always(r==x, "A mismatch");
      d->read(B,&r,8);  assert_always(r==y, "B mismatch");
      d->read(C,&r,8);  assert_always(r==z, "C mismatch");
      // Grouped at t=0: print just one line for all HAL ops in this cycle
      log("\n");
      return true;
    }
    if (S == "hal_bounds") {
      uint64_t base=d->get_base(), sz=d->get_size(), r=1;
      d->read(base-8,&r,8);        assert_always(r==0, "below-base not zero");
      d->read(base+sz-8,&r,8);     (void)r; // define expected if preseeded
      // Grouped at t=0: single line for both reads
      log("\n");
      return true;
    }
    return false;
  };
  if (is_hal) {
    // For HAL runs, execute only at t=0 via DRAM HAL (no Sim::run, no driver traffic)
    bool ok = run_hal(S, soc);
    assert_always(ok, "unknown -suite (hal_*)");
  }

  // **************
  // Step 8: Layer 2 — protocol/timing suites (MemTester drives MemCtrl)
  // Requires -driver=test
  // **************
  auto run_suite = [&](const std::string& s, SoC& soc, int mem_lat) -> bool {
    if (s == "proto_core") {
      // Core issues its smoke sequence; no explicit assertions here.
      return true;
    }
    // 1) Proto_accel_sum: core-driven test of accelerator sum protocol 
    // exercises: Tile1 issues CUSTOM-0 → AccelArraySumSoc runs → AccelMemBridge talks to MemCtrl → Dram
    if ((s == "proto_accel_sum") || (s == "proto_accel_sum_altaddr") || (s == "proto_accel_sum_badarg")) {
      // enforce the right topology
      assert_always(!use_tester, "proto_accel_sum requires core driver (use_test_driver=false)");
      assert_always(soc.dram_ != nullptr, "proto_accel_sum: missing DRAM");
      assert_always(soc.core_ != nullptr, "proto_accel_sum: missing Tile1Core");
      assert_always(soc.mem_ != nullptr, "proto_accel_sum: missing MemCtrl");
      // 2) reset sim to clean starting point
      Sim::reset(); 
      // 3) define tiny instr encoders (lambdas): build RV32 instr words (uint32_t) from fields (rd, rs1, rs2, imm, etc.)
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
      auto encode_ebreak = []() -> uint32_t {
        return 0x00100073u;
      };
      auto encode_ecall = []() -> uint32_t {
        return 0x00000073u;
      };
      auto encode_custom0 = [](uint32_t rd, uint32_t rs1, uint32_t rs2, uint32_t funct3) -> uint32_t {
        return (0x00u << 25) | ((rs2 & 0x1fu) << 20) | ((rs1 & 0x1fu) << 15) |
               ((funct3 & 0x7u) << 12) | ((rd & 0x1fu) << 7) | 0x0bu;
      };
      // emits usual lui+addi pair to load 32-b immediate into rd
      auto emit_li = [&](std::vector<uint32_t>& out, uint32_t rd, uint32_t value) {
        const uint32_t hi = (value + 0x800u) >> 12;
        const int32_t lo = static_cast<int32_t>(value) - static_cast<int32_t>(hi << 12);
        assert_always(lo >= -2048 && lo <= 2047, "proto_accel_sum: addi immediate out of range");
        out.push_back(encode_lui(rd, hi));
        out.push_back(encode_addi(rd, rd, lo));
      };
      // 4) choose addresses and explain the two address spaces
      // prog written into DRAM at dram_base + prog_base
      // PC set to prog_base (because Tile1Core::DramMemoryPort maps CPU address addr → DRAM dram_base + addr)
      // Mailbox is a CPU addr 0x100 that program stores into, and TB reads it back physically at dram_base + 0x100
      // Array base passed to CUSTOM-0 is also a CPU address; AccelMemBridge adds dram_base for MemCtrl requests.
      const uint32_t prog_base    = 0x200u;
      const uint32_t mailbox_addr = 0x100u;  // Tile1 CPU byte address; maps to DRAM base + 0x100 via DramMemoryPort
      const uint32_t array_addr   = (s == "proto_accel_sum_altaddr") ? 0x6000u :
                                    ((s == "proto_accel_sum_badarg") ? 0x4002u : 0x4000u); // Tile1 CPU byte address for array base
      const uint32_t init_base_cpu = (s == "proto_accel_sum_badarg") ? 0x4000u : array_addr; // keep DRAM test-data init aligned for badarg suite
      const uint32_t len_words    = 16u;
      const uint64_t mailbox_phys = cpu_to_phys(soc, mailbox_addr);
      const uint64_t prog_phys    = cpu_to_phys(soc, prog_base);
      // 6) initialize test data array in DRAM
      uint32_t expected_sum = 0;
      for (uint32_t i = 0; i < len_words; ++i) {
        const uint32_t v = i + 1u;
        expected_sum += v;
        const uint64_t phys = cpu_to_phys(soc, init_base_cpu + (4u * i));
        soc.dram_->write(phys, &v, sizeof(v));
      }
      
      uint32_t mailbox_init = 0u; // clear mailbox
      soc.dram_->write(mailbox_phys, &mailbox_init, sizeof(mailbox_init));  // DRAM holds accelerator for reading, clean mailbox, (soon) the program
      // 6) builds tiny RV32 program image: call accel once, store result, exit
      std::vector<uint32_t> prog;
      prog.reserve(16);
      emit_li(prog, 10u, array_addr);                    // a0 = base (arg0)
      emit_li(prog, 11u, len_words);                     // a1 = len  (arg1)
      prog.push_back(encode_custom0(12u, 10u, 11u, 0u)); // a2 = custom0 array-sum
      emit_li(prog, 5u, mailbox_addr);                   // t0 = mailbox cpu addr
      prog.push_back(encode_sw(12u, 5u, 0));             // sw a2, 0(t0)
      prog.push_back(encode_addi(17u, 0u, 93));          // a7 = 93 (exit syscall)
      prog.push_back(encode_addi(10u, 0u, 0));           // a0 = exit code 0
      prog.push_back(encode_ecall());                    // exit
      (void)encode_ebreak; // helper kept for local encoding completeness
      // 7) write program into DRAM via HAL, set PC to start of program
      for (size_t i = 0; i < prog.size(); ++i) {
        const uint64_t addr = prog_phys + static_cast<uint64_t>(4u * i);
        const uint32_t word = prog[i];
        soc.dram_->write(addr, &word, sizeof(word));
      }

      soc.core_->set_pc(prog_base);
      // 8) run cycles until program completes (by ecall exit); program will store result into mailbox, which TB checks after completion
      constexpr int kMaxCycles = 2000;
      for (int i = 0; i < kMaxCycles; ++i) {
        Sim::run();
        log("\n");
      }

      uint32_t got = 0;
      soc.dram_->read(mailbox_phys, &got, sizeof(got));
      if (s == "proto_accel_sum_badarg") {
        assert_always(got == 3u, "proto_accel_sum_badarg: expected ACCEL_E_BADARG");
        assert_always(got != expected_sum, "proto_accel_sum_badarg: unexpectedly matched valid sum");
      } else {
        assert_always(got == expected_sum, "proto_accel_sum: mailbox mismatch");
      }
      const uint32_t expected = (s == "proto_accel_sum_badarg") ? 3u : expected_sum;
      std::cout << s << ": PASS got=0x" << std::hex << got
                << " expected=0x" << expected << std::dec << std::endl;
      return true;
    }
    if (!use_tester || !soc.tester_ || !soc.dram_) return false;
    auto* t = soc.tester_;
    auto base = soc.dram_->get_base();
    auto A = base + 0x100;
    auto B = base + 0x108;
    if (s == "proto_raw") {
      t->clear_script(); t->clear_results();
      t->enqueue_store(A, 0xDEADBEEFULL);
      t->enqueue_load(A);
      for (int i=0;i<10;i++) { Sim::run(); log("\n"); }
      const auto& rs = t->results();
      // Expect at least one response for the LOAD (last event should be load)
      assert_always(!rs.empty(), "raw: no responses observed");
      const auto &e = rs.back();
      assert_always(e.is_load, "raw: last event is not a load");
      assert_always((int64_t)(e.resp_cyc - e.sent_cyc) == 0, "raw: expected same-tick response");
      return true;
    }
    if (s == "proto_no_raw") {
      t->clear_script(); t->clear_results();
      t->enqueue_store(A, 0xABCD1234ULL);
      t->enqueue_load(B);
      for (int i=0;i<mem_lat+6;i++) { Sim::run(); log("\n"); }
      const auto& rs = t->results();
      assert_always(!rs.empty(), "no_raw: no responses observed");
      // last event should be LOAD(B)
      const auto &e = rs.back();
      assert_always(e.is_load, "no_raw: last event is not a load");
      int64_t delta = (int64_t)(e.resp_cyc - e.sent_cyc);
      assert_always(delta == (mem_lat + 1) || delta == mem_lat, "no_raw: expected mem_latency(+1) cycles");
      return true;
    }
    if (s == "proto_rar") {
      t->clear_script(); t->clear_results();
      // initialize A first
      t->enqueue_store(A, 0xCAFEBABECAFED00DULL);
      t->enqueue_load(A);
      t->enqueue_load(A);
      for (int i=0;i<mem_lat+8;i++) { Sim::run(); log("\n"); }
      const auto& rs = t->results();
      // Expect at least two load responses at the end
      assert_always(rs.size() >= 2, "rar: insufficient responses");
      const auto &e1 = rs[rs.size()-2];
      const auto &e2 = rs[rs.size()-1];
      assert_always(e1.is_load && e2.is_load, "rar: expected two loads");
      assert_always((uint64_t)e1.rdata == (uint64_t)e2.rdata, "rar: load values mismatch");
      return true;
    }
    if (s == "proto_lat") {
      for (int L : {0,1,3,7}) {
        soc.set_mem_latency(L); Sim::run(); log("\n");
        t->clear_script(); t->clear_results();
        t->enqueue_store(A, 0x12345678ULL);
        t->enqueue_load(B);
        for (int i=0;i<L+6;i++) { Sim::run(); log("\n"); }
        const auto& rs = t->results();
        assert_always(!rs.empty(), "latency_sweep: no responses observed");
        const auto &e = rs.back();
        assert_always(e.is_load, "latency_sweep: last event is not a load");
        int64_t delta = (int64_t)(e.resp_cyc - e.sent_cyc);
        assert_always(delta == (L + 1) || delta == L, "latency_sweep: expected mem_latency(+1) cycles");
      }
      return true;
    }
    return false;
  };
  if (is_proto) {
    bool ok2 = run_suite(S, soc, eff_lat);
    assert_always(ok2, "unknown or failed -suite (proto_*)");
  }

  // **************
  // Step 9: Run cycles — batch (-steps=N) or interactive (Enter to step)
  // For HAL suites, no cycles are run (t=0 only), so exit now.
  // **************
  if (is_hal) {
    return 0;
  }
  if (steps > 0) {
    for (int i = 0; i < steps; ++i) {
      Sim::run();
      log("\n");
    }
    if (drain) { // optional fence at end if -drain
      // Advance until all posted stores drain from MemCtrl (useful for fences)
      while (!soc.mem_->writes_empty()) { Sim::run(); log("\n"); }
    }
    return 0;
  }

  for (;;) { // interactive
    char buff[64];
    printf("> ");
    if (!fgets(buff, 64, stdin)) break;
    if (*buff == 'q')
      break;
    else if (*buff == '0')
      Sim::reset();
    else if (*buff == 'f') { // fence: drain posted stores
      // Keep stepping until MemCtrl reports no pending stores remain
      while (!soc.mem_->writes_empty()) { Sim::run(); }
    }
    else
      Sim::run();
    log("\n");
  }
  return 0;
}
