// **********************************************************************
// smicro/src/AccelMemBridge.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 16 2026
/*
Tiny MemCtrl client shim for accelerators to facilitate more realistic SoC-memory paths for accelerators.

Sits on the MemReq/MemResp protocol boundary (same as RvCore / MemTester).
Accelerator (or other host) calls start_load32/start_store32(), and this bridge
converts those 32-bit operations into MemCtrl-compatible 8-byte MemReq traffic.
Load32 issues one aligned 64-bit load and selects a 32-bit lane.
Store32 performs an internal RMW sequence (load64 -> merge lane -> store64).

  host API 
  (how AccelArraySumSoc.cpp talks to this bridge)
  +-------------------- AccelMemBridge --------------------+
->| start_load32()  |                                      |
->| start_store32() |                                      |
->| resp_consume()  | update() pushes one MemReq -> m_req  |==>
<-| can_accept()    | update() pops one MemResp <- m_resp  |<==
<-| resp_valid()    |                                      |
<-| resp_data()     | update() communicates w/ host API    |
  +--------------------------------------------------------+

Typical smicro wiring today (SoC.cpp, use_test_driver_ == false):

--- AccelMemBridge --+   +-------------- MemCtrl ---------------+   +-- Dram --
        ,---m_req -->|==>| in_core_req   update_issue()   s_req |==>| s_req 
update()             |   |                                      |   |         
        '--m_resp <--|<==| out_core_resp update_retire() s_resp |<==| s_resp
---------------------+   +--------------------------------------+   +----------

Notes / v1 constraints:
- Single-outstanding: one host operation at a time; internal RMW uses sequential
  requests while the bridge remains busy.
- Blocking-style completion: resp_valid() stays true until resp_consume().
- Stores also wait for a MemResp "ack" (assumes MemCtrl returns a completion resp).
- The Tile1 core in smicro is *not* on this MemCtrl path yet; Tile1Core still talks
  directly to DRAM via its private MemoryPort shim. This bridge exists so accelerators
  can exercise the SoC memory timing path through MemCtrl before the core LSU is ported.
*/

#pragma once
#include <cascade/Cascade.hpp>
#include "smem/MemTypes.hpp"
#include <cstdint>

class AccelMemBridge : public Component {
  DECLARE_COMPONENT(AccelMemBridge);
public:
  AccelMemBridge(std::string name, COMPONENT_CTOR);

  Clock(clk);
  // FifoOutput and FifoInput declare ports backed by FIFO channels
  FifoOutput(MemReq,  m_req);   
  FifoInput (MemResp, m_resp);

  // Host-facing non-blocking API (single outstanding operation)
  // true iff no operation is active and no unconsumed response is latched.
  bool can_accept() const;

  void start_load32(uint32_t addr);
  void start_store32(uint32_t addr, uint32_t data);

  bool     resp_valid() const;
  uint32_t resp_data() const;
  void     resp_consume();
  void     set_addr_base(uint64_t base) { addr_base_ = base; }
  uint64_t addr_base() const { return addr_base_; }

  void update();
  void reset();

private:
  enum class OpKind : uint8_t {
    NONE,
    LOAD32,
    STORE32
  };

  enum class Phase : uint8_t {
    IDLE,
    ISSUE_LOAD64,
    WAIT_LOAD64_RESP,
    ISSUE_STORE64,
    WAIT_STORE64_ACK
  };

  OpKind op_kind_ = OpKind::NONE;
  Phase phase_    = Phase::IDLE;

  // Captured host request context.
  uint64_t addr_base_    = 0;     // physical base added to CPU-style byte addresses
  uint64_t aligned_addr_ = 0;
  bool upper_lane_       = false; // false: [31:0], true: [63:32]
  uint32_t store_data32_ = 0;     // used for STORE32
  uint64_t rmw_word64_   = 0;     // merged store payload after RMW load

  // Sticky response latch (must be consumed explicitly)
  bool resp_valid_    = false;
  uint32_t resp_data_ = 0;
};
