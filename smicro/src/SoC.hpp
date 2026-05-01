// **********************************************************************
// smicro/src/SoC.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 16 2025
/*
The top-level system wiring.

------- RVCore ---------+   +-------------- MemCtrl ---------------+   +--- Dram ---
 update_req() ->  m_req |==>| in_core_req   update_issue()   s_req |==>| s_req 
                        |   |                                      |   |       
update_resp() <- m_resp |<==| out_core_resp update_retire() s_resp |<==| s_resp
------------------------+   +--------------------------------------+   +------------


*/
#pragma once

#include <cascade/Cascade.hpp>
#include "smem/MemTypes.hpp"
#include "AccelCmd.hpp"
#include "SmicroTypes.hpp" 
#include "RvCore.hpp"
#include "L1.hpp"
#include "L2.hpp"
#include "smem/Dram.hpp"
#include "smem/MemCtrl.hpp"
#include "NnAccel.hpp"
#include "MemTester.hpp"
#include "Tile1Core.hpp" // wrapper for Tile1 RISC-V core
#include <string>

class AccelPort;
class AccelMemBridge;
class AccelArraySumSoc;

using namespace Cascade; // ok in project headers (macros expect it), but avoid in sub-component headers

class SoC : public Component {
  DECLARE_COMPONENT(SoC);

public:
  SoC(AttachMode mode, bool use_test_driver, COMPONENT_CTOR); // constructor with configurable mode
  ~SoC() override;                                            // destructor

  // External interface ports
  FifoOutput(AccelCmd, accel_cmd_out); // two external
  FifoInput (bit,      accel_done_in); // accel ports

  // Clock input (externally driven)
  Clock(clk);

  void update();
  void reset();
  
  // TB hooks
  void set_mem_latency(int v) { if (mem_) mem_->set_latency(v); }            // set MemCtrl latency in cycles
  void set_dram_latency(int v) { set_mem_latency(v); }                       // back-compat alias
  void set_posted_writes(bool en) { if (mem_) mem_->set_posted_writes(en); } // enable/disable posted write acks

  void attach_accelerator(AccelPort* accel);

  // Submodules (owned by SoC)
  // RvCore  *core_ = nullptr;
  Tile1Core *core_             = nullptr;
  AccelMemBridge *ab_          = nullptr;
  AccelArraySumSoc *array_sum_ = nullptr;
  MemTester *tester_           = nullptr;
  L1      *l1_                 = nullptr;
  L2      *l2_                 = nullptr;
  smem::Dram *dram_            = nullptr;
  MemCtrl *mem_                = nullptr;
  NnAccel *accel_              = nullptr;

private:
  AttachMode mode_;
  bool use_test_driver_ = false;
  // Add more internal state as needed
};
