// **********************************************************************
// smicro/src/MemTester.hpp
// **********************************************************************

#pragma once
#include <cascade/Cascade.hpp>
#include "MemTypes.hpp"
#include <vector>
#include <unordered_map>

class MemTester : public Component {
  DECLARE_COMPONENT(MemTester);
public:
  MemTester(std::string name, COMPONENT_CTOR);

  Clock(clk);

  // Memory master ports (same shape as RvCore)
  FifoOutput(MemReq,  m_req);
  FifoInput (MemResp, m_resp);

  // Scripted ops
  enum Kind : uint8_t { LOAD=0, STORE=1 };
  struct Op { Kind kind; uint64_t addr; uint64_t data; uint16_t size; };

  // Result record (one per response observed)
  struct Ev { uint16_t id; bool is_load; uint64_t sent_cyc; uint64_t resp_cyc; u64 rdata; };

  // Host helpers to build scripts and inspect results
  void clear_script();
  void clear_results();
  void enqueue_store(uint64_t addr, uint64_t data, uint16_t size=8);
  void enqueue_load(uint64_t addr, uint16_t size=8);
  const std::vector<Ev>& results() const { return results_; }

  void update_issue();   // reads internal state, writes m_req
  void update_retire();  // reads m_resp, writes internal state
  void reset();

private:
  // Simple per-cycle timebase for stamping events
  uint64_t cyc_ = 0;

  // Script and pc
  std::vector<Op> script_;
  size_t pc_ = 0;

  // ID allocator and pending map (id -> partial event)
  uint16_t next_id_ = 0;
  struct Pending { bool is_load; uint64_t sent_cyc; };
  std::unordered_map<uint16_t, Pending> pending_;

  // Completed results
  std::vector<Ev> results_;
};
