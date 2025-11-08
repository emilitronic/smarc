// **********************************************************************
// smicro/src/RvCore.cpp
// **********************************************************************
// S Magierowski Aug 16 2025
/*
Write -> readback smoke: store pattern to test_addr_, wait for ack,
then issue a read, check resp against pattern, and park.
*/

#include "RvCore.hpp"

RvCore::RvCore(std::string /*name*/, IMPL_CTOR) { // constructor registers two update fns. so req/resp paths can be 0-delay w/o comp loops
  UPDATE(update_req).writes(m_req);
  UPDATE(update_resp).reads(m_resp);
}

void RvCore::update_req() {
  switch (state_) {
    case S_IDLE: {
      if (m_req.full()) break;
      MemReq w{}; w.addr = (uint64_t)test_addr_; w.write = true; w.size = 8; w.wdata = (uint64_t)pattern_;
      m_req.push(w);
      trace("core: sent store @0x%llx = 0x%llx", (unsigned long long)w.addr, (unsigned long long)w.wdata);
      state_ = S_W_SENT;
      break;
    }
    case S_R_REQ: {
      if (m_req.full()) break;
      MemReq r{}; r.addr = (uint64_t)test_addr_; r.write = false; r.size = 8; r.wdata = 0;
      m_req.push(r);
      trace("core: sent load  @0x%llx", (unsigned long long)r.addr);
      state_ = S_R_WAIT;
      break;
    }
    default: break;
  }
}

void RvCore::update_resp() {
  switch (state_) {
    case S_W_SENT: {
      if (m_resp.empty()) break;      // wait for write ack
      (void)m_resp.pop();
      state_ = S_R_REQ;
      break;
    }
    case S_R_WAIT: {
      if (m_resp.empty()) break;
      auto resp = m_resp.pop();
      bool ok = (resp.rdata == (uint64_t)pattern_);
      trace("core: got resp 0x%llx  %s", (unsigned long long)resp.rdata, ok?"OK":"MISMATCH");
      state_ = S_DONE;
      break;
    }
    default: break;
  }
}

// Default update function is unused; kept to satisfy DECLARE_COMPONENT
void RvCore::update() {}

void RvCore::reset() {
  state_ = S_IDLE;
}
