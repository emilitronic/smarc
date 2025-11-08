// **********************************************************************
// smicro/src/NnAccel.hpp
// **********************************************************************
// S Magierowski Aug 16 2025
// 
#pragma once
#include <cascade/Cascade.hpp>
#include "MemTypes.hpp"
#include "AccelCmd.hpp"
#include "SmicroTypes.hpp"

class NnAccel : public Component {
  DECLARE_COMPONENT(NnAccel);
public:
  NnAccel(std::string name, AttachMode mode, COMPONENT_CTOR);
  Clock(clk);
  // Control
  FifoInput (AccelCmd, cmd_in);
  FifoOutput(bit,      done);
  // Memory master
  FifoOutput(MemReq,   m_req);
  FifoInput (MemResp,  m_resp);

  // HAL methods
  void set_src_dst(u64 a, u64 b, u64 c, u32 n); // config accel w/ addrs & size of vectors
  void kick();    // starts accels computations
  bool is_done(); // allows testbench to check if accelerator has finished

// accelerator's internal state
private:
  AttachMode mode_;
  u64 a_addr_ = 0;
  u64 b_addr_ = 0;
  u64 c_addr_ = 0;
  u32 n_ = 0;
  bool busy_ = false;

  void update();
  void reset();
};