// **********************************************************************
// smile/include/AccelArraySumMc.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 21 2026

#pragma once

#include "AccelPort.hpp"

class MemoryPort;

// Multi-cycle array-sum accelerator (CUSTOM-0 funct3=0).
// Processes one 32-bit load per tick() using MemoryPort request/response.
class AccelArraySumMc : public AccelPort {
public:
  explicit AccelArraySumMc(MemoryPort& mem);

  void issue(uint32_t raw_inst,
             uint32_t pc,
             uint32_t rs1_val,
             uint32_t rs2_val) override;
  void tick() override;

  bool has_response() const override;
  uint32_t read_response() override;

  uint32_t mem_load32(uint32_t addr) override;
  void mem_store32(uint32_t addr, uint32_t data) override;

private:
  MemoryPort& mem_;

  bool busy_ = false;
  bool has_resp_ = false;
  uint32_t resp_ = 0;

  uint32_t base_ = 0;
  uint32_t len_ = 0;
  uint32_t idx_ = 0;
  uint32_t sum_ = 0;
  bool waiting_mem_ = false;
};

