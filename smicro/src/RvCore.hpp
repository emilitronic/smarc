// **********************************************************************
// smicro/src/RvCore.hpp
// **********************************************************************
// S Magierowski Aug 16 2025
/*
------- RVCore ---------+   
 update_req() ->  m_req |==>
                        |   
update_resp() <- m_resp |<==
------------------------+   
*/

#pragma once
#include <cascade/Cascade.hpp>
#include "MemTypes.hpp" // mem req/resp packet types

class RvCore : public Component {
  DECLARE_COMPONENT(RvCore);
public:
  RvCore(std::string name, COMPONENT_CTOR);

  Clock(clk);

  // Memory master
  FifoOutput(MemReq,  m_req);  // o/p queue carries req's to mem; push to it & check full() or freeCount()
  FifoInput (MemResp, m_resp); // i/p queue carries resp's from mem; pop from it & check empty() or popCount()

  void update();
  void reset();
  void update_req();
  void update_resp();

private:
  enum { S_IDLE, S_W_SENT, S_R_REQ, S_R_WAIT, S_DONE } state_ = S_IDLE;
  u64 test_addr_ = 0x80000008ull;
  u64 pattern_   = 0xA5A5A5A5DEADBEEFull;
};
