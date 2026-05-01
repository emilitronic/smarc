// **********************************************************************
// smile/include/AccelArraySumMc.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 21 2026
/*
Memory-aware AccelPort implementation that interprets CUSTOM-0 funct3=0 as
"sum rs2 32-bit words starting at rs1", but computes the result over multiple
cycles. The accelerator issues one memory read at a time via MemoryPort's
request/response interface and publishes a sticky response once complete.
*/

#pragma once

#include "AccelPort.hpp"

namespace smem { class MemoryPort; }

// AccelArraySumMc implements the AccelPort protocol by interpreting CUSTOM-0
// instructions as "sum an array of 32-bit words from memory", one load per tick.
class AccelArraySumMc : public AccelPort {
public:
  explicit AccelArraySumMc(smem::MemoryPort& mem);

  // Command path: CUSTOM-0 request from Tile1.
  void issue(uint32_t raw_inst,
             uint32_t pc,
             uint32_t rs1_val,
             uint32_t rs2_val) override;
  // Progress path: advances in-flight accumulation each cycle.
  void tick() override;

  // Response path: rd write-back for Tile1 (sticky until read_response()).
  bool has_response() const override;
  uint32_t read_response() override;

  // Memory client view: request/response data path plus direct proxy helpers.
  uint32_t mem_load32(uint32_t addr) override;
  void mem_store32(uint32_t addr, uint32_t data) override;

private:
  smem::MemoryPort& mem_;

  // Accelerator command/response state.
  bool busy_ = false;
  bool has_resp_ = false;
  uint32_t resp_ = 0;

  // Array-sum state for current command.
  uint32_t base_ = 0;
  uint32_t len_ = 0;
  uint32_t idx_ = 0;
  uint32_t sum_ = 0;
  // True after request_read32() until its response is consumed.
  bool waiting_mem_ = false;
};
