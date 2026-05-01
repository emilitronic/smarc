// **********************************************************************
// smile/include/AccelArraySum.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 22 2025
/*
Memory-aware AccelPort implementation that interprets CUSTOM-0 as:
- funct3=0: “sum rs2 32-bit words starting at rs1” and respond in the same cycle.
For v1 contract compliance it returns ACCEL_E_UNSUPPORTED for funct3!=0 and ACCEL_E_BADARG for misaligned rs1.
It logs each request, walks DRAM synchronously via the memory hooks, and exposes the blocking load/store API so future accelerators can reuse it
*/

#pragma once

#include "AccelPort.hpp"

namespace smem { class MemoryPort; }

// AccelArraySum implements the AccelPort protocol by interpreting CUSTOM-0
// instructions as "sum an array of 32-bit words from memory".
class AccelArraySum : public AccelPort {
public:
  explicit AccelArraySum(smem::MemoryPort& mem);

  // Command path: CUSTOM-0 request from Tile1.
  void issue(uint32_t raw_inst,
             uint32_t pc,
             uint32_t rs1_val,
             uint32_t rs2_val) override;

  // Response path: rd write-back for Tile1.
  bool has_response() const override;
  uint32_t read_response() override;

  // Memory client view: DRAM-as-L1.
  uint32_t mem_load32(uint32_t addr) override;
  void mem_store32(uint32_t addr, uint32_t data) override;

private:
  smem::MemoryPort& mem_;
  bool has_resp_ = false;
  uint32_t resp_ = 0;
};
