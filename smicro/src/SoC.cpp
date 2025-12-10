
// **********************************************************************
// smicro/src/SoC.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 16 2025
/*
Smoke-test path (no caches)

------- RVCore ---------+   +-------------- MemCtrl ---------------+   +--- Dram ---
 update_req() ->  m_req |==>| in_core_req   update_issue()   s_req |==>| s_req 
                        |   |                                      |   |       
update_resp() <- m_resp |<==| out_core_resp update_retire() s_resp |<==| s_resp
------------------------+   +--------------------------------------+   +------------
*/
#include "SoC.hpp"

using namespace Cascade;

// Optional global for HALs/tests
SoC* g_soc = nullptr;

SoC::SoC(AttachMode mode, bool use_test_driver, IMPL_CTOR)
  : mode_(mode), use_test_driver_(use_test_driver)
{
  g_soc = this;
  // ---- Allocate blocks ----
  // core_   = new RvCore("core");
  core_   = new Tile1Core("core");
  tester_ = new MemTester("tester");
  l1_     = new L1("l1");
  l2_     = new L2("l2");
  dram_   = new Dram("dram", /*latency cycles*/ 0);
  mem_    = new MemCtrl("mem");
  accel_  = new NnAccel("accel", mode);

  // ---- Clocking ----
  core_->clk << clk; tester_->clk << clk; l1_->clk << clk; l2_->clk << clk; dram_->clk << clk; mem_->clk << clk; accel_->clk << clk;

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

  // ---- Smoke-test wiring: bypass caches/accel; wire core & tester ----
  // Core/TestMaster <-> MemCtrl
  if (use_test_driver_) {
    // Disable core ports when tester is driving MemCtrl
    core_->m_req.sendToBitBucket();
    core_->m_resp.wireToZero();
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
    mem_->in_core_req.wireToZero();        //      0 -> mem ctrl input: seen as permanently empty by MemCtrl
    mem_->out_core_resp.sendToBitBucket(); // bucket <- mem ctrl output: any responses are discarded
  }

  // MemCtrl <-> DRAM (DRAM is zero-latency storage) 
  dram_->s_req        << mem_->s_req;      //           mem ctrl -> dram
  mem_->s_resp        << dram_->s_resp;    //           mem ctrl <- dram
  // 0/0 delays end-to-end; MemCtrl owns timing
  mem_->in_core_req.setDelay(0);
  mem_->out_core_resp.setDelay(0);
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

SoC::~SoC() {
  g_soc = nullptr;    // invalidate global first to avoid dangling global during child deletes
  delete accel_;
  delete mem_;
  delete dram_;
  delete l2_;
  delete l1_;
  delete tester_;
  delete core_;
}
