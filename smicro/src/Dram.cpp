// **********************************************************************
// smicro/src/Dram.cpp
// **********************************************************************
// S Magierowski Aug 16 2025
/*
   +--- Dram ---
==>| s_req 
   |       
<==| s_resp
   +------------
*/
#include "Dram.hpp"
#include <cstring>

Dram::Dram(std::string /*name*/, int latency, IMPL_CTOR) // DRAM constructor
  : latency_(latency)
{
  UPDATE(update).reads(s_req).writes(s_resp); // hint let's Cascade order producer->consumer correctly
  mem_.resize(256 * 1024 * 1024);             // upon construction resizes (allocates) mem_ to 256MB of DRAM
}

// Set latency in cycles (applies to the next accepted request; in-flight unaffected)
void Dram::set_latency(int v) {
  if (v < 0) v = 0;
  latency_ = v;
  trace("dram: latency=%d", latency_);
}

// method: allocate space in DRAM, a very simple "bump allocator"
void* Dram::alloc(uint64_t bytes) {
  // NOTE: This is a simplified allocation scheme that doesn't handle alignment or deallocation.
  // (i.e., can't free memory) It also doesn't check for running out of memory.
  void* addr = (void*)(base_addr_ + next_addr_); // return address in global address space
  next_addr_ += bytes;            // then "bumps" next_addr_ up by number of bytes requested
  return addr;                    // returns starting addr
}

// method: write to DRAM, use memcpy fn. to copy data between host program's memory and simulated DRAM (mem_ vector)
void Dram::write(uint64_t addr, const void* src, uint64_t bytes) {
  // Trace HAL-side writes for visibility in -test=multi/bounds
  if (bytes >= 8) {
    uint64_t tmp = 0; std::memcpy(&tmp, src, 8);
    trace("dram_hal: write addr=0x%llx size=%llu data=0x%016llx\n",
          (unsigned long long)addr,
          (unsigned long long)bytes,
          (unsigned long long)tmp);
  } else {
    trace("dram_hal: write addr=0x%llx size=%llu\n",
          (unsigned long long)addr,
          (unsigned long long)bytes);
  }
  if (addr < base_addr_) return;
  uint64_t off = addr - base_addr_;
  if (off + bytes > mem_.size()) return;
  memcpy(&mem_[off], src, bytes);
}
// method: read from DRAM for HAL
void Dram::read(uint64_t addr, void* dst, uint64_t bytes) {
  // OOB below base → zero-fill and return
  if (addr < base_addr_) {
    std::memset(dst, 0, (size_t)bytes);
    return;
  }
  // Compute offset once (safe after the check above)
  uint64_t off_u64 = addr - base_addr_;
  size_t   sz      = mem_.size();
  size_t   off     = (size_t)off_u64;
  size_t   n       = (size_t)bytes;
  // OOB past end (overflow-safe): if n > sz - off, range doesn’t fit
  if (off > sz || n > (sz - off)) {
    std::memset(dst, 0, n);
    return;
  }
  // In-bounds copy
  std::memcpy(dst, mem_.data() + off, n);
  // Trace a preview of the first 8 bytes (same as before)
  if (n >= 8) {
    uint64_t tmp = 0;
    std::memcpy(&tmp, dst, 8);
    trace("dram_hal: read  addr=0x%llx size=%llu data=0x%016llx\n",
          (unsigned long long)addr,
          (unsigned long long)bytes,
          (unsigned long long)tmp);
  } else {
    trace("dram_hal: read  addr=0x%llx size=%llu\n",
          (unsigned long long)addr,
          (unsigned long long)bytes);
  }
}

void Dram::update() {
  // Zero-latency storage with 1-entry read hold; writes produce no responses.
  if (!hold_valid_ && !s_req.empty()) {          // accept one req (if not already holding a LOAD req)
    auto rq = s_req.pop();
    if (rq.write) {                                // if req=STORE copy wdata into to byte array; no sig on s_resp
      if (rq.addr >= base_addr_) {
        uint64_t off = rq.addr - base_addr_;
        if (off + rq.size <= mem_.size())
          std::memcpy(&mem_[off], &rq.wdata, rq.size);
      }
    } else {                                       // if req=LOAD put req in hold_ (1-entry latch)
      hold_ = rq;
      hold_valid_ = true;
    }
  }

  if (hold_valid_ && !s_resp.full()) {           // if holding a LOAD req and resp FIFO has space, respond now
    MemResp resp{};                                // build zero-initialized resp
    if (hold_.addr >= base_addr_) {                // if addr inside DRAM window
      uint64_t off = hold_.addr - base_addr_;
      if (off + hold_.size <= mem_.size())          // bounds check (if requested bytes fit)
        std::memcpy(&resp.rdata, &mem_[off], hold_.size);
      else
        resp.rdata = 0;
    } else {
      resp.rdata = 0;
    }
    resp.id = hold_.id;                            // preserve req ID for matching 
    s_resp.push(resp);                             // send response to memory controller
    hold_valid_ = false;                           // clear the 1-entry LOAD req latch; we’re no longer holding a read
  }
}

void Dram::reset() {
  next_addr_ = 0;
  cnt_ = -1;
  // Preload memory location with the expected test pattern
  uint64_t v = 0x1122334455667788ull;
  std::memcpy(&mem_[0], &v, 8);
}
