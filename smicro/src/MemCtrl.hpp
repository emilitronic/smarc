// **********************************************************************
// smicro/src/MemCtrl.hpp
// **********************************************************************
// S Magierowski Aug 22 2025
/*
  --> in_core_req   -> update_issue()  ->  s_req -->
  <-- out_core_resp <- update_retire() <- s_resp <--
*/

#pragma once
#include <cascade/Cascade.hpp>
#include <deque>
#include "MemTypes.hpp"

class MemCtrl : public Component {
  DECLARE_COMPONENT(MemCtrl);
public:
  MemCtrl(std::string name, COMPONENT_CTOR);

  Clock(clk);

  // Master ports (for now: core only)
  FifoInput (MemReq,  in_core_req);
  FifoOutput(MemResp, out_core_resp);

  // DRAM side
  FifoOutput(MemReq,  s_req);
  FifoInput (MemResp, s_resp);

  void update_issue();   // reads in_*, writes s_req
  void update_retire();  // reads s_resp, writes out_*
  void reset();
  // Fence helper: returns true when no pending stores remain in the pipeline
  bool writes_empty() const;
  void set_posted_writes(bool en) { posted_writes_ = en; }

  void set_latency(int v) { if (v < 0) v = 0; latency_ = v; trace("mem: latency=%d", latency_); }

private:
  struct Q { MemReq r; int cnt; };
  std::deque<Q> pipe_; // pipeline stages to model latency
  int latency_ = 0;
  // helpers
  bool find_pending_store(u64 addr, u16 size, u64 &val) const;
  bool posted_writes_ = true; // if false, ack store when it drains to DRAM
};
