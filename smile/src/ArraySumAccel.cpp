// **********************************************************************
// smile/src/ArraySumAccel.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 22 2025
//
// Array-sum accelerator implementation used by tb_tile1.
// CUSTOM-0 is interpreted as:
//   rs1 = base address (byte address, 4-byte aligned)
//   rs2 = length in 32-bit elements
//   rd  = destination for the sum

#include "ArraySumAccel.hpp"
#include "Tile1.hpp"

#include <iomanip>
#include <iostream>

ArraySumAccel::ArraySumAccel(MemoryPort& mem)
  : mem_(mem) {
}

void ArraySumAccel::issue(uint32_t raw_inst,
                          uint32_t pc,
                          uint32_t rs1_val,
                          uint32_t rs2_val) {
  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  std::cout << "[ARRAYSUM] pc=0x"
            << std::hex << std::setw(8) << pc
            << " inst=0x" << std::setw(8) << raw_inst
            << " base=0x" << std::setw(8) << rs1_val
            << " len="    << std::dec << rs2_val
            << std::endl;

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);

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

bool ArraySumAccel::has_response() const {
  return has_resp_;
}

uint32_t ArraySumAccel::read_response() {
  has_resp_ = false;
  return resp_;
}

uint32_t ArraySumAccel::mem_load32(uint32_t addr) {
  return mem_.read32(addr);
}

void ArraySumAccel::mem_store32(uint32_t addr, uint32_t data) {
  mem_.write32(addr, data);
}
