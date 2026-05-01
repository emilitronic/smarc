// **********************************************************************
// smile/include/AccelDemoAdd.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 21 2025

// Simple demo accelerator that logs requests and proxies memory access.

#pragma once

#include "AccelPort.hpp"

namespace smem { class MemoryPort; }

class AccelDemoAdd : public AccelPort {
public:
  explicit AccelDemoAdd(smem::MemoryPort& mem);

  void issue(uint32_t raw_inst,
             uint32_t pc,
             uint32_t rs1_val,
             uint32_t rs2_val) override;

  bool has_response() const override;
  uint32_t read_response() override;

  uint32_t mem_load32(uint32_t addr) override;
  void mem_store32(uint32_t addr, uint32_t data) override;

private:
  smem::MemoryPort& mem_;
  bool has_resp_ = false;
  uint32_t resp_ = 0;
};
