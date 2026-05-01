// **********************************************************************
// smem/include/smem/DramMemoryPort.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 30 2026
/*
 Shared DRAM-backed MemoryPort adapter for smile/smicro.
*/
#pragma once

#include "smem/MemoryPort.hpp"

#include <cstdint>

namespace smem {

class Dram;

class DramMemoryPort : public MemoryPort {
public:
  explicit DramMemoryPort(Dram& dram);

  uint32_t read32(uint32_t addr) override;
  void write32(uint32_t addr, uint32_t value) override;

  void cycle() override;
  bool can_request() const override;
  void request_read32(uint32_t addr) override;
  void request_write32(uint32_t addr, uint32_t value) override;
  bool resp_valid() const override;
  uint32_t resp_data() const override;
  void resp_consume() override;

private:
  Dram& dram_;
  bool resp_valid_ = false;
  uint32_t resp_data_ = 0;
};

} // namespace smem
