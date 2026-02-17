// **********************************************************************
// smicro/src/AccelArraySumSoc.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 17 2026

#include "AccelArraySumSoc.hpp"
#include "AccelMemBridge.hpp"

#include <cassert>

AccelArraySumSoc::AccelArraySumSoc(AccelMemBridge& ab)
  : ab_(ab) {
}

void AccelArraySumSoc::tick() {
  if (has_resp_) {
    return;
  }

  if (!busy_) {
    return;
  }

  if (!waiting_load_) {
    if (idx_ == len_) {
      resp_ = sum_;
      has_resp_ = true;
      busy_ = false;
      return;
    }

    if (ab_.can_accept()) {
      ab_.start_load32(base_ + 4u * idx_);
      waiting_load_ = true;
    }
    return;
  }

  if (ab_.resp_valid()) {
    sum_ += ab_.resp_data();
    ab_.resp_consume();
    idx_++;
    waiting_load_ = false;
  }
}

void AccelArraySumSoc::issue(uint32_t raw_inst,
                             uint32_t pc,
                             uint32_t rs1_val,
                             uint32_t rs2_val) {
  (void)pc;

  if (busy_ || has_resp_) {
    resp_ = ACCEL_E_BUSY;
    has_resp_ = true;
    return;
  }

  const uint32_t funct3 = (raw_inst >> 12) & 0x7u;
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
  waiting_load_ = false;
  busy_ = true;
}

bool AccelArraySumSoc::has_response() const {
  return has_resp_;
}

uint32_t AccelArraySumSoc::read_response() {
  has_resp_ = false;
  return resp_;
}

uint32_t AccelArraySumSoc::mem_load32(uint32_t addr) {
  (void)addr;
  assert(false && "AccelArraySumSoc::mem_load32 is not used");
  return 0;
}

void AccelArraySumSoc::mem_store32(uint32_t addr, uint32_t data) {
  (void)addr;
  (void)data;
  assert(false && "AccelArraySumSoc::mem_store32 is not used");
}
