// **********************************************************************
// smile/src/AccelDemoAdd.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 21 2025

// Demo accelerator implementation used by tb_tile1.

#include "AccelDemoAdd.hpp"
#include "Tile1.hpp"

#include <iomanip>
#include <iostream>

AccelDemoAdd::AccelDemoAdd(smem::MemoryPort& mem) : mem_(mem) {}

void AccelDemoAdd::issue(uint32_t raw_inst,
                         uint32_t pc,
                         uint32_t rs1_val,
                         uint32_t rs2_val) {
  std::ios_base::fmtflags old_flags = std::cout.flags();
  char old_fill = std::cout.fill('0');

  std::cout << "[ACCEL] pc=0x"
            << std::hex << std::setw(8) << pc
            << " inst=0x" << std::setw(8) << raw_inst
            << " rs1=0x" << std::setw(8) << rs1_val
            << " rs2=0x" << std::setw(8) << rs2_val
            << std::dec << std::endl;

  std::cout.fill(old_fill);
  std::cout.flags(old_flags);

  resp_ = rs1_val + rs2_val;
  has_resp_ = true;
}

bool AccelDemoAdd::has_response() const {
  return has_resp_;
}

uint32_t AccelDemoAdd::read_response() {
  has_resp_ = false;
  return resp_;
}

uint32_t AccelDemoAdd::mem_load32(uint32_t addr) {
  return mem_.read32(addr);
}

void AccelDemoAdd::mem_store32(uint32_t addr, uint32_t data) {
  mem_.write32(addr, data);
}
