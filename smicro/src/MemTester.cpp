// **********************************************************************
// smicro/src/MemTester.cpp
// **********************************************************************

#include "MemTester.hpp"

using namespace Cascade;

MemTester::MemTester(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update_issue).writes(m_req);
  UPDATE(update_retire).reads(m_resp);
}

void MemTester::clear_script() { script_.clear(); pc_ = 0; }
void MemTester::clear_results() { results_.clear(); pending_.clear(); }

void MemTester::enqueue_store(uint64_t addr, uint64_t data, uint16_t size) {
  script_.push_back(Op{STORE, addr, data, size});
}
void MemTester::enqueue_load(uint64_t addr, uint16_t size) {
  script_.push_back(Op{LOAD, addr, 0ull, size});
}

void MemTester::update_issue() {
  // Bump cycle counter once per tick
  cyc_++;

  // Issue at most one op per cycle if available and downstream can accept
  if (pc_ < script_.size() && !m_req.full()) {
    const Op &op = script_[pc_];
    MemReq r{};
    r.addr  = (u64)op.addr;
    r.size  = (u16)op.size;
    r.id    = (u16)next_id_++;
    if (op.kind == STORE) {
      r.write = true;
      r.wdata = (u64)op.data;
      pending_[r.id] = Pending{false, cyc_};
    } else {
      r.write = false;
      pending_[r.id] = Pending{true, cyc_};
    }
    m_req.push(r);
    pc_++;
  }
}

void MemTester::update_retire() {
  if (!m_resp.empty()) {
    auto rr = m_resp.pop();
    auto it = pending_.find((uint16_t)rr.id);
    if (it != pending_.end()) {
      Ev e{};
      e.id = (uint16_t)rr.id;
      e.is_load = it->second.is_load;
      e.sent_cyc = it->second.sent_cyc;
      e.resp_cyc = cyc_;
      e.rdata = rr.rdata;
      results_.push_back(e);
      pending_.erase(it);
    }
  }
}

void MemTester::reset() {
  cyc_ = 0;
  pc_ = 0;
  next_id_ = 0;
  script_.clear();
  results_.clear();
  pending_.clear();
}
