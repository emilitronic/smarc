// **********************************************************************
// smicro/src/Dram.hpp
// **********************************************************************
// S Magierowski Aug 16 2025

// note: u64 in Cascade is a bitvec<64>, not a built-in integer. 
// It isn’t a literal type, so you can’t use it in constexpr

/*
   +--- Dram ---
==>| s_req 
   |       
<==| s_resp
   +------------
*/

#pragma once
#include <cascade/Cascade.hpp>
#include "MemTypes.hpp"
#include <vector>

class Dram : public Component {
  DECLARE_COMPONENT(Dram);
public:
  Dram(std::string name, int latency, COMPONENT_CTOR);
  Clock(clk);
  FifoInput (MemReq,  s_req);
  FifoOutput(MemResp, s_resp);
  void set_latency(int v);   // Set DRAM latency in cycles. Applies to the next accepted request (in-flight unaffected).

  // HAL/test helpers
  uint64_t get_base() const { return base_addr_; }
  uint64_t get_size() const { return mem_.size(); }
  // methods for HAL to call
  void* alloc(uint64_t bytes);
  void  write(uint64_t addr, const void* src, uint64_t bytes);
  void  read(uint64_t addr, void* dst, uint64_t bytes);

private:
  int latency_ = 0;
  std::vector<char> mem_;  // storage for simulated DRAM, C++ vector of characters (bytes)
  u64 next_addr_ = 0;      // counter to keep track of next available memory address
  static constexpr uint64_t base_addr_ = 0x80000000ull; // base address mapped to mem_[0]

  void update();
  void reset();

  // request/response pipeline state for smoke test
  int    cnt_ = -1;
  MemReq cur_{};
  // Read hold (1-entry) to handle backpressure cleanly
  bool   hold_valid_ = false;
  MemReq hold_{};
};
