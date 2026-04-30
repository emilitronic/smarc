// **********************************************************************
// smem/src/DramMemoryPort.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 30 2026
#include "smem/DramMemoryPort.hpp"

namespace smem {

DramMemoryPort::DramMemoryPort(Dram& dram) : dram_(dram) {}

uint32_t DramMemoryPort::read32(uint32_t addr) {
  uint32_t value = 0;
  const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr);
  dram_.read(phys, &value, sizeof(value));
  return value;
}

void DramMemoryPort::write32(uint32_t addr, uint32_t value) {
  const uint64_t phys = dram_.get_base() + static_cast<uint64_t>(addr);
  dram_.write(phys, &value, sizeof(value));
}

void DramMemoryPort::cycle() {}

bool DramMemoryPort::can_request() const {
  return !resp_valid_;
}

void DramMemoryPort::request_read32(uint32_t addr) {
  resp_data_ = read32(addr);
  resp_valid_ = true;
}

void DramMemoryPort::request_write32(uint32_t addr, uint32_t value) {
  write32(addr, value);
  resp_data_ = 0;
  resp_valid_ = true;
}

bool DramMemoryPort::resp_valid() const {
  return resp_valid_;
}

uint32_t DramMemoryPort::resp_data() const {
  return resp_data_;
}

void DramMemoryPort::resp_consume() {
  resp_valid_ = false;
}

} // namespace smem
