
// **********************************************************************
// smicro/src/SoC.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 16 2025
/*
Smoke-test topologies (no caches; accel off)

(1) Suite: proto_core        (Driver: core, MemCtrl idle)

    Tile1Core (Tile1 CPU)
    ---------------------
      Tile1::tick()
          |
          |  MemoryPort::read32 / write32
          v
      [ Tile1Core::DramMemoryPort ]
          |
          |  Dram::read / Dram::write
          v
         ------ 
        | Dram |   (zero-latency storage; HAL-style interface)
         ------

    - MemCtrl core-side ports are neutralized:
        core.m_req        -> bit bucket
        core.m_resp       <- 0
        mem.in_core_req   <- 0
        mem.out_core_resp -> bit bucket
    - MemTester is instantiated but not used.


(2) Suites: proto_raw / proto_no_raw / proto_rar / proto_lat   (Driver: tester)

    -------- MemTester -------+   +-------------- MemCtrl ---------------+   +--- Dram ---
     enqueue_*() ->  m_req    |==>| in_core_req   update_issue()   s_req |==>| s_req 
                              |   |                                      |   |       
       results() <- m_resp    |<==| out_core_resp update_retire() s_resp |<==| s_resp
    --------------------------+   +--------------------------------------+   +------------

    - Tile1Core’s MemReq/MemResp ports are parked:
        core.m_req  -> bit bucket
        core.m_resp <- 0
      so the core is not on the MemCtrl protocol path yet.
    - Tile1Core may still have a private MemoryPort → Dram connection for core experiments,
      but all protocol / latency tests are driven by MemTester through MemCtrl.

Planned evolution:
    Later, Tile1Core will grow a small LSU that drives m_req/m_resp directly, replacing
    MemTester as the MemCtrl client so the real core exercises the same MemReq/MemResp path.
*/
#include "SoC.hpp"
#include "AccelMemBridge.hpp"
#include "AccelArraySumSoc.hpp"

using namespace Cascade;

// Optional global for HALs/tests
SoC* g_soc = nullptr;

SoC::SoC(AttachMode mode, bool use_test_driver, IMPL_CTOR)
  : mode_(mode), use_test_driver_(use_test_driver)
{
  g_soc = this;
  // ---- Allocate blocks ----
  // core_   = new RvCore("core");
  core_      = new Tile1Core("core");      // Tile1Core: minimal wrapper to host Tile1 in smicro
  ab_        = new AccelMemBridge("ab");   // bridge between accel & MemCtrl (implements MemReq/MemResp ifc on one side, and custom accel-friendly ifc on the other)
  array_sum_ = new AccelArraySumSoc(*ab_); // accel model obj (pass bridge for mem ld/st)
  tester_    = new MemTester("tester");
  l1_        = new L1("l1");
  l2_        = new L2("l2");
  dram_      = new smem::Dram("dram", /*latency cycles*/ 0);
  mem_       = new smem::MemCtrl("mem");
  accel_     = new NnAccel("accel", mode);
  ab_->set_addr_base(dram_->get_base());

  // ---- Clocking ----
  core_->clk << clk; ab_->clk << clk; tester_->clk << clk; l1_->clk << clk; l2_->clk << clk; dram_->clk << clk; mem_->clk << clk; accel_->clk << clk;

  // // ---- Smoke-test wiring: bypass caches/accel; wire core <-> DRAM directly ----
  // // Core/TestMaster <-> MemCtrl
  // if (use_test_driver_) {
  //   // Disable core ports
  //   core_->m_req.sendToBitBucket();                  //   core -> bucket      | 
  //   core_->m_resp.wireToZero();                      //   core <- 0           | use
  //   // Enable tester                                                          |
  //   mem_->in_core_req   << tester_->m_req;           // tester -> mem ctrl    | tester (not core)
  //   tester_->m_resp     << mem_->out_core_resp;      // tester <- mem ctrl    |
  // } else {
  //   // Disable tester ports
  //   tester_->m_req.sendToBitBucket();                // tester -> bucket      |
  //   tester_->m_resp.wireToZero();                    // tester <- 0           | use
  //   // Enable core                                                            |
  //   mem_->in_core_req   << core_->m_req;             //   core -> mem ctrl    | core (not tester)
  //   core_->m_resp       << mem_->out_core_resp;      //   core <- mem ctrl    |
  // }
  
  // ---- Connect Tile1Core directly to DRAM via its internal MemoryPort shim ----
  core_->attach_dram(dram_); // let Tile1Core know which DRAM to talk to
  attach_accelerator(array_sum_); // connect accel to Tile1

  // ---- Smoke-test wiring: bypass caches/accel; wire core & tester ----
  // Core/TestMaster <-> MemCtrl
  if (use_test_driver_) {
    // Disable core ports when tester is driving MemCtrl
    core_->m_req.sendToBitBucket();
    core_->m_resp.wireToZero();
    // Disable bridge ports when tester is driving MemCtrl
    ab_->m_req.sendToBitBucket();
    ab_->m_resp.wireToZero();
    // Tester -> MemCtrl
    mem_->in_core_req   << tester_->m_req;
    tester_->m_resp     << mem_->out_core_resp;
  } else {
    // No tester: neutralize its ports
    tester_->m_req.sendToBitBucket();
    tester_->m_resp.wireToZero();
    // For now, Tile1Core talks directly to DRAM (via attach_dram),
    // so we do not connect it to MemCtrl's core-side ports.
    core_->m_req.sendToBitBucket();        //   core -> bucket
    core_->m_resp.wireToZero();            //   core <- 0
    // Bridge -> MemCtrl (accelerator memory bridge is the active MemCtrl client)
    mem_->in_core_req << ab_->m_req;
    ab_->m_resp       << mem_->out_core_resp;
  }

  // MemCtrl <-> DRAM (DRAM is zero-latency storage) 
  dram_->s_req        << mem_->s_req;      //           mem ctrl -> dram
  mem_->s_resp        << dram_->s_resp;    //           mem ctrl <- dram
  // Break req/resp combinational feedback when bridge drives MemCtrl directly.
  if (use_test_driver_) {
    mem_->in_core_req.setDelay(0);
    mem_->out_core_resp.setDelay(0);
  } else {
    mem_->in_core_req.setDelay(1);
    mem_->out_core_resp.setDelay(1);
  }
  mem_->s_req.setDelay(0);
  mem_->s_resp.setDelay(0);

  // Neutralize unused L1/L2/accel internal ports so construction checks pass
  l1_->up_req.sendToBitBucket();     l1_->up_req.wireToZero();
  l1_->up_resp.sendToBitBucket();    l1_->up_resp.wireToZero();
  l1_->down_req.sendToBitBucket();   l1_->down_req.wireToZero();
  l1_->down_resp.sendToBitBucket();  l1_->down_resp.wireToZero();
  l2_->core_req.sendToBitBucket();   l2_->core_req.wireToZero();
  l2_->core_resp.sendToBitBucket();  l2_->core_resp.wireToZero();
  l2_->mem_req.sendToBitBucket();    l2_->mem_req.wireToZero();
  l2_->mem_resp.sendToBitBucket();   l2_->mem_resp.wireToZero();
  l2_->accel_req.sendToBitBucket();  l2_->accel_req.wireToZero();
  l2_->accel_resp.sendToBitBucket(); l2_->accel_resp.wireToZero();

  // Accel attach (ViaL2 by default)
  // Make top-level accel control ports inert unless TB connects them
  accel_cmd_out.sendToBitBucket(); 
  accel_done_in.wireToZero();
  // Neutralize accel control sink until TB supplies commands
  accel_->cmd_in.sendToBitBucket(); accel_->cmd_in.wireToZero();
  // Neutralize accel done source until TB consumes it
  accel_->done.sendToBitBucket();   accel_->done.wireToZero();
  // Neutralize accel memory FIFOs for smoke test
  accel_->m_req.sendToBitBucket();  accel_->m_req.wireToZero();
  accel_->m_resp.sendToBitBucket(); accel_->m_resp.wireToZero();
}

void SoC::update() {
  trace("soc: tick\tmode=%d (%s) \t", (int)mode_, attachModeName(mode_));
}

void SoC::reset() {
  // No state yet
}
// helper to connect accel to Tile1Core's accel port
void SoC::attach_accelerator(AccelPort* accel) {
  if (core_) core_->attach_accelerator(accel); // Tile1Core::attach_accelerator() calls tile_.attach_accelerator(accel)
}

SoC::~SoC() {
  g_soc = nullptr;   // invalidate global first to avoid dangling global during child deletes
  delete accel_;
  delete array_sum_; // run destructor to free accl obj (before bridge since it has a ref to the bridge)
  delete ab_;
  delete mem_;
  delete dram_;
  delete l2_;
  delete l1_;
  delete tester_;
  delete core_;
}
