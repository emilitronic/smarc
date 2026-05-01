// **********************************************************************
// smile/src/AccelArraySumMc.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 21 2026
/*
Multi-cycle array-sum accelerator implementation used by tb_tile1.
CUSTOM-0 is interpreted as:
  funct3 = 0 -> array sum
  rs1 = base address (byte address, 4-byte aligned)
  rs2 = length in 32-bit elements
  rd  = destination for the sum

Unlike AccelArraySum, this implementation advances through the array in tick(),
issuing one read request at a time and consuming one response at a time.
*/

#include "AccelArraySumMc.hpp"
#include "Tile1.hpp"

AccelArraySumMc::AccelArraySumMc(smem::MemoryPort& mem)
  : mem_(mem) {
}

void AccelArraySumMc::issue(uint32_t raw_inst,
                            uint32_t /*pc*/,
                            uint32_t rs1_val,
                            uint32_t rs2_val) {
  // v1 verb select: funct3
  const uint32_t funct3 = (raw_inst >> 12) & 0x7u;

  // Keep response sticky semantics: if previous work/response is pending,
  // return ACCEL_E_BUSY instead of starting a new command.
  if (busy_ || has_resp_) {
    resp_ = ACCEL_E_BUSY;
    has_resp_ = true;
    return;
  }
  // Only funct3=0 is supported by this accelerator.
  if (funct3 != 0u) {
    resp_ = ACCEL_E_UNSUPPORTED;
    has_resp_ = true;
    return;
  }
  // Sum is defined on 32-bit aligned words.
  if ((rs1_val & 0x3u) != 0u) {
    resp_ = ACCEL_E_BADARG;
    has_resp_ = true;
    return;
  }

  // Arm multi-cycle job state.
  base_ = rs1_val;
  len_ = rs2_val;
  idx_ = 0;
  sum_ = 0;
  waiting_mem_ = false;
  busy_ = true;
}

void AccelArraySumMc::tick() {
  if (!busy_ || has_resp_) return;

  // Done: publish final sum as sticky response.
  if (idx_ == len_) {
    resp_ = sum_;
    has_resp_ = true;
    busy_ = false;
    waiting_mem_ = false;
    return;
  }

  // Phase A: issue one read when the memory port can accept a request.
  if (!waiting_mem_) {
    if (mem_.can_request()) {
      mem_.request_read32(base_ + 4u * idx_);
      waiting_mem_ = true;
    }
    return;
  }

  // Phase B: consume one read response and advance to the next index.
  if (mem_.resp_valid()) {
    sum_ += mem_.resp_data();
    mem_.resp_consume();
    idx_++;
    waiting_mem_ = false;
  }
}

bool AccelArraySumMc::has_response() const {
  return has_resp_;
}

uint32_t AccelArraySumMc::read_response() {
  has_resp_ = false;
  return resp_;
}

uint32_t AccelArraySumMc::mem_load32(uint32_t addr) {
  return mem_.read32(addr);
}

void AccelArraySumMc::mem_store32(uint32_t addr, uint32_t data) {
  mem_.write32(addr, data);
}
