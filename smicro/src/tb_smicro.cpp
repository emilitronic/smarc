// **********************************************************************
// smicro/src/tb_smicro.cpp
// **********************************************************************
// S Magierowski Aug 16 2025
/*
SoC test harness with a single-switch suite to avoid ambiguity.
Suites: hal_* run only DRAM HAL at t=0 (no driver traffic). proto_* run protocol timing via core or tester.

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
#include <iostream>
#include "SoC.hpp"
#include "AccelCmd.hpp" 

using namespace std;

// **************
// Parameters (CLI flags): name, default value, help text
// **************
StringParameter(topo,       "via_l2", "Topology: via_l1|via_l2|dram|priv"); // defaults topo is via_l2
IntParameter(steps,          0,      "Batch steps; 0=interactive");
// New single-switch suite
StringParameter(suite,      "proto_core", "Suite: hal_none|hal_multi|hal_bounds|proto_core|proto_raw|proto_no_raw|proto_rar|proto_lat");
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
  bool use_tester = (S != "proto_core") && is_proto;        // tester for proto_* except proto_core
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
