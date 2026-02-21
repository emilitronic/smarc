// **********************************************************************
// smile/src/AccelArraySumMc.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 21 2026

#include "AccelArraySumMc.hpp"
#include "Tile1.hpp"

AccelArraySumMc::AccelArraySumMc(MemoryPort& mem)
  : mem_(mem) {
}

void AccelArraySumMc::issue(uint32_t raw_inst,
                            uint32_t /*pc*/,
                            uint32_t rs1_val,
                            uint32_t rs2_val) {
  const uint32_t funct3 = (raw_inst >> 12) & 0x7u;

  if (busy_ || has_resp_) {
    resp_ = ACCEL_E_BUSY;
    has_resp_ = true;
    return;
  }
  if (funct3 != 0u) {
    resp_ = ACCEL_E_UNSUPPORTED;
    has_resp_ = true;
    return;
  }
  if ((rs1_val & 0x3u) != 0u) {
    resp_ = ACCEL_E_BADARG;
    has_resp_ = true;
    return;
  }

  base_ = rs1_val;
  len_ = rs2_val;
  idx_ = 0;
  sum_ = 0;
  waiting_mem_ = false;
  busy_ = true;
}

void AccelArraySumMc::tick() {
  if (!busy_ || has_resp_) return;

  if (idx_ == len_) {
    resp_ = sum_;
    has_resp_ = true;
    busy_ = false;
    waiting_mem_ = false;
    return;
  }

  if (!waiting_mem_) {
    if (mem_.can_request()) {
      mem_.request_read32(base_ + 4u * idx_);
      waiting_mem_ = true;
    }
    return;
  }

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

