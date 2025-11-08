// **********************************************************************
// smicro/src/MemCtrl.cpp
// **********************************************************************
// S Magierowski Aug 22 2025
/*
Does three simple things each cycle.  
1) Keeps a queue with a mem latency countdown per request. 
  - Decrement a countdown on every queued item (not the one added this cycle).
2) If the front item is ready (its countdown=0), send it to DRAM and remove from queue.
3) Take at most one new STORE or LOAD request from core. 

Some details:
• core can make either STORE or LOAD reaquest (obviously)
• STOREs can be "posted" or "non-posted"
  - posted STORE means MemCtrl give core ACK as soon as it queues it up to send to DRAM
  - non-posted STORE means core gets ACK only afer DRAM gets store
• LOADs can fetch from STORE queue
  - if a LOAD matches a STORE in the queue, return that value to core right away (and still send that STORE to DRAM)
*/

#include "MemCtrl.hpp"

MemCtrl::MemCtrl(std::string /*name*/, IMPL_CTOR) {  // constructor registers two update fns. & says what they touch
  UPDATE(update_issue).reads(in_core_req).writes(s_req);
  UPDATE(update_retire).reads(s_resp).writes(out_core_resp);
}

// ----- first update: accepts from core, ages/queues, issues to DRAM -----
void MemCtrl::update_issue() {
  // 1) Age existing entries (do not age the one we may enqueue this tick)
  for (auto &q : pipe_) if (q.cnt > 0) --q.cnt; // pipe_ holdes queued memory ops
  // 2) Sending signals to DRAM
  if (!pipe_.empty() && pipe_.front().cnt == 0) {  // if head of queue is matured
    const MemReq &hq = pipe_.front().r;
    if (hq.write && !posted_writes_) {               // if non-posted STORE @ head (i.e., ACK not sent to core yet)
      if (!out_core_resp.full() && !s_req.full()) {    // if MemCtrl & DRAM FIFOs can take data
        MemResp ack{}; ack.rdata = 0; ack.id = hq.id; ack.err = 0; // build a STORE ACK
        out_core_resp.push(ack);                                   // send ACK to core now
        s_req.push(hq);                                            // issue STORE to DRAM
        pipe_.pop_front();                                         // remove from queue
      }
    } else if (!s_req.full()) {                      // if LOAD or posted STORE @ head (STORE ACK already sent to core)
      s_req.push(hq);                                  // issue to DRAM
      pipe_.pop_front();                               // remove from queue
    }
  }
  // 3) Take at most one new request from core; posted write-ack + RAW handling
  if (!in_core_req.empty()) {       // if core has REQ ready 
    if (out_core_resp.full()) return; // avoid pop if we might need to ACK a store but cannot (being convervative)
    auto r = in_core_req.pop();       // take REQ from core
    if (r.write) {                    // *** if core's REQ is STORE ***
      assert_always(((u64)r.size == 8) && (((u64)r.addr & 7ull) == 0ull), "MemCtrl posted write: only 8-byte aligned ops supported for now"); // Guard
      if (posted_writes_) {                                       // if posted STORE
        MemResp ack{}; ack.rdata = 0; ack.id = r.id; ack.err = 0;   // build ACK
        out_core_resp.push(ack);                                    // send ACK to core now
      }
      pipe_.push_back(Q{r, latency_});                            // put STORE in latency queue
    } else {                          // *** if core's REQ is LOAD ***
      u64 fwd = 0;
      if (find_pending_store((u64)r.addr, (u16)r.size, fwd)) {   // check if a queued STORE=LOAD (store hazard); if so forward full word; partial size handling can be added later
        assert_always(((u64)r.size == 8) && (((u64)r.addr & 7ull) == 0ull), "MemCtrl RAW forward: only 8-byte aligned ops supported for now"); // Guard
        MemResp rr{}; rr.rdata = fwd; rr.id = r.id; rr.err = 0;    // build synthetic LOAD response with STORE's data
        out_core_resp.push(rr);                                    // return data to core now (no DRAM access)
      } else {                                                   // normal path through latency pipe
        pipe_.push_back(Q{r, latency_});                           // no hazard: queue the read for timed issue to DRAM
      }
    }
  }
}

// ----- second update: passes DRAM results back to core -----
void MemCtrl::update_retire() {
  if (!s_resp.empty() && !out_core_resp.full()) { // if DRAM returns LOAD & core can take it
    auto rr = s_resp.pop();                         // get DRAM's resp
    MemResp o{}; o.rdata = rr.rdata; o.id = rr.id; o.err = 0; // build o/p resposne to core
    out_core_resp.push(o);                          // send resp to core 
  }
}

// clear state
void MemCtrl::reset() {
  pipe_.clear(); // forget all queued resuests
}

// small helpers
// deal with LOAD = queued STORE (store hazard); scan most-recent-first for a pending write that overlaps [addr, addr+size)
bool MemCtrl::find_pending_store(u64 addr, u16 size, u64 &val) const {
  if (size == 0) return false;                           // empty LOAD size is a miss
  u64 a0 = addr;                                         // read range start
  u64 a1 = addr + (u64)size;                             // read range end (exclusive)
  for (int i = (int)pipe_.size() - 1; i >= 0; --i) {     // search newset --> oldest
    const Q &q = pipe_[(size_t)i];                         // candidate entry
    if (!q.r.write) continue;                              // only consider STOREs
    u64 b0 = (u64)q.r.addr;                                // STORE range start
    u64 b1 = b0 + (u64)q.r.size;                           // STORE range end (exlucsive)
    bool overlap = !(a1 <= b0 || b1 <= a0);                // overlap test
    if (overlap) { val = (u64)q.r.wdata; return true; }    // on hit: return STORE data
  }
  return false;                                         // no pending STORE covers this LOAD
}

// true when no STOREs remain in the latency queue (used for fences)
bool MemCtrl::writes_empty() const {
  for (const auto &q : pipe_) if (q.r.write) return false; // if any queued request is a store, not empty
  return true;                                             // otherwise all stores are drained
}
