// **********************************************************************
// smile/src/AccelArraySum.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 22 2025
/*
Array-sum accelerator implementation used by tb_tile1.
CUSTOM-0 is interpreted as:
  rs1 = base address (byte address, 4-byte aligned)
  rs2 = length in 32-bit elements
  rd  = destination for the sum

This accelerator does all of its work in issue() and returns the result immediately when issue() completes. 
This is simpler than the multi-cycle version (AccelArraySumMc) but less realistic as an accelerator model, 
since it does not allow for any overlap of accelerator execution with other tile work or for modeling of 
latency while the accelerator is working. The multi-cycle version is also more flexible in how it can be 
extended to support more complex behaviors (e.g., streaming access patterns, non-blocking command issue, etc.)  
*/
#include "AccelArraySum.hpp"
#include "Tile1.hpp"

#include <iomanip>
#include <iostream>

AccelArraySum::AccelArraySum(smem::MemoryPort& mem)
  : mem_(mem) {
}

void AccelArraySum::issue(uint32_t raw_inst,
                          uint32_t pc,
                          uint32_t rs1_val,
                          uint32_t rs2_val) {
  // v1 verb select: funct3
  const uint32_t funct3 = (raw_inst >> 12) & 0x7u;

  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  std::cout << "[ARRAYSUM] pc=0x"
            << std::hex << std::setw(8) << pc
            << " inst=0x" << std::setw(8) << raw_inst
            << " base=0x" << std::setw(8) << rs1_val
            << " len="    << std::dec << rs2_val
            << " f3="     << funct3
            << std::endl;

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);

  // If a previous response is still pending, refuse the new command.
  if (has_resp_) {
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

  const uint32_t base = rs1_val;
  const uint32_t len  = rs2_val;

  uint32_t sum = 0;
  for (uint32_t i = 0; i < len; ++i) {
    const uint32_t addr = base + 4u * i;
    const uint32_t val  = mem_.read32(addr);
    sum += val;
  }

  resp_ = sum;
  has_resp_ = true;
}

bool AccelArraySum::has_response() const {
  return has_resp_;
}

uint32_t AccelArraySum::read_response() {
  has_resp_ = false;
  return resp_;
}

uint32_t AccelArraySum::mem_load32(uint32_t addr) {
  return mem_.read32(addr);
}

void AccelArraySum::mem_store32(uint32_t addr, uint32_t data) {
  mem_.write32(addr, data);
}
