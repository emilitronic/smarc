// **********************************************************************
// smicro/src/AccelMemBridge.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 16 2026

#include "AccelMemBridge.hpp"

using namespace Cascade;

AccelMemBridge::AccelMemBridge(std::string /*name*/, IMPL_CTOR) {
  /* UPDATE macro registers AccelMemBridge::update() w/ Cascade's scheduler
  as a callback so it will be called once per sim cycle. Chained call declares 
  .writes() and .reads() Casecade methods to be able to pop m_resp FIFO and 
  push m_req FIFO.  Currently, we allow both actions to happen in same cycle. */
  UPDATE(update).reads(m_resp).writes(m_req); 
}                                                    

bool AccelMemBridge::can_accept() const {
  return (phase_ == Phase::IDLE) && !resp_valid_;
}

// queue up the load 
void AccelMemBridge::start_load32(uint32_t addr) {
  assert_always(can_accept(), "AccelMemBridge::start_load32 called while busy");
  assert_always((addr & 0x3u) == 0u, "AccelMemBridge::start_load32 requires 4-byte alignment");
  // require addr to be in one of 32-bit lanes of 8-byte word
  assert_always(((addr & 0x7u) == 0u) || ((addr & 0x7u) == 4u), "AccelMemBridge::start_load32 lane must be +0 or +4");

  aligned_addr_ = static_cast<uint64_t>(addr & ~0x7u); // compute aligned 64-b addr
  upper_lane_   = ((addr >> 2) & 0x1u) != 0;           // which lane do we want 0 [31:0] or 1 [63:32]
  store_data32_ = 0;                                   // clear store…
  rmw_word64_   = 0;                                   // …related scratch (not used for load but just to be safe)
  op_kind_      = OpKind::LOAD32;
  phase_        = Phase::ISSUE_LOAD64;                 // push 64-b load
}

// queue up the store (implemented as read-modify-write (RMW) under the hood since MemCtrl is 64-bit)
void AccelMemBridge::start_store32(uint32_t addr, uint32_t data) {
  assert_always(can_accept(), "AccelMemBridge::start_store32 called while busy");
  assert_always((addr & 0x3u)  == 0u, "AccelMemBridge::start_store32 requires 4-byte alignment");
  assert_always(((addr & 0x7u) == 0u) || ((addr & 0x7u) == 4u), "AccelMemBridge::start_store32 lane must be +0 or +4");

  aligned_addr_ = static_cast<uint64_t>(addr & ~0x7u);
  upper_lane_   = ((addr >> 2) & 0x1u) != 0;
  store_data32_ = data;
  rmw_word64_   = 0;
  op_kind_      = OpKind::STORE32;
  phase_        = Phase::ISSUE_LOAD64;
}

bool AccelMemBridge::resp_valid() const {
  return resp_valid_;
}

uint32_t AccelMemBridge::resp_data() const {
  return resp_data_;
}

void AccelMemBridge::resp_consume() {
  resp_valid_ = false; // clear sticky response valid; caller must explicitly consume each response
  resp_data_  = 0;
}

void AccelMemBridge::update() {
  // emit aligned 8B load request (for LOAD32 or STORE32-RMW) and advance to waiting
  if (phase_ == Phase::ISSUE_LOAD64 && !m_req.full()) {
    MemReq req{};
    req.addr  = static_cast<u64>(aligned_addr_);
    req.wdata = static_cast<u64>(0);
    req.size  = static_cast<u16>(8); // MemCtrl requires 8-byte granularity
    req.write = false;
    req.id    = static_cast<u16>(0);

    m_req.push(req);
    phase_ = Phase::WAIT_LOAD64_RESP;
  }
  // consume load response; either finish LOAD32 (select lane) or compute merged word for STORE32
  if (phase_ == Phase::WAIT_LOAD64_RESP && !m_resp.empty()) {
    const MemResp resp         = m_resp.pop();
    const uint64_t loaded_word = static_cast<uint64_t>(resp.rdata);

    if (op_kind_ == OpKind::LOAD32) {
      resp_data_ = upper_lane_
        ? static_cast<uint32_t>((loaded_word >> 32) & 0xffffffffull)
        : static_cast<uint32_t>(loaded_word & 0xffffffffull);
      resp_valid_ = true;
      op_kind_    = OpKind::NONE;
      phase_      = Phase::IDLE;
    } else if (op_kind_ == OpKind::STORE32) {
      const uint64_t lane_mask = upper_lane_ ? 0xffffffff00000000ull : 0x00000000ffffffffull;
      const uint64_t lane_data = upper_lane_
        ? (static_cast<uint64_t>(store_data32_) << 32)
        : static_cast<uint64_t>(store_data32_);
      rmw_word64_ = (loaded_word & ~lane_mask) | lane_data;
      phase_ = Phase::ISSUE_STORE64;
    } else {
      assert_always(false, "AccelMemBridge internal error: WAIT_LOAD64_RESP without active op");
    }
  }
  // emit aligned 8B store request with merged RMW payload and advance to waiting for ACK
  if (phase_ == Phase::ISSUE_STORE64 && !m_req.full()) {
    MemReq req{};
    req.addr  = static_cast<u64>(aligned_addr_);
    req.wdata = static_cast<u64>(rmw_word64_);
    req.size  = static_cast<u16>(8); // MemCtrl requires 8-byte granularity
    req.write = true;
    req.id    = static_cast<u16>(0);

    m_req.push(req);
    phase_ = Phase::WAIT_STORE64_ACK;
  }
  // consume store ACK response and publish completion to host via sticky resp_valid_
  if (phase_ == Phase::WAIT_STORE64_ACK && !m_resp.empty()) {
    (void)m_resp.pop(); // ACK response (payload ignored for store completion)
    resp_data_  = 0;
    resp_valid_ = true;
    op_kind_    = OpKind::NONE;
    phase_      = Phase::IDLE;
  }
}

void AccelMemBridge::reset() {
  op_kind_      = OpKind::NONE;
  phase_        = Phase::IDLE;
  aligned_addr_ = 0;
  upper_lane_   = false;
  store_data32_ = 0;
  rmw_word64_   = 0;
  resp_valid_   = false;
  resp_data_    = 0;
}
