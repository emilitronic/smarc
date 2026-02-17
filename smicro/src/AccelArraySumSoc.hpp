// **********************************************************************
// smicro/src/AccelArraySumSoc.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 17 2026
/*
AccelArraySumSoc: a toy accelerator model for smicro that implements the shared
AccelPort (CUSTOM-0) contract using the real SoC memory path.

Purpose:
- Acts like a small “device” attached to Tile1: Tile1 issues CUSTOM-0 (funct3=0)
  with rs1=base address and rs2=length, then blocks until the accelerator responds.
- While the core is stalled, this accelerator advances cycle-by-cycle via tick().

How it works:
- issue(): validates verb/alignment and captures (base,len), then marks the op busy.
- tick(): repeatedly schedules one 32-bit load at a time using AccelMemBridge
  (AccelMemBridge emits MemReq/MemResp traffic into MemCtrl), accumulates a
  uint32_t sum, and finally publishes a sticky response (sum) to the core.
- Completion is reported through has_response()/read_response() per AccelPort v1.

Key point:
- This is not software running on Tile1; it is a simulated accelerator “hardware”
  block that performs memory reads through MemCtrl timing and returns a result
  back to Tile1 via the CUSTOM-0 interface.
*/
#pragma once

#include "AccelPort.hpp"

#include <cstdint>

class AccelMemBridge;

class AccelArraySumSoc : public AccelPort {
public:
  explicit AccelArraySumSoc(AccelMemBridge& ab);

  void tick() override;

  void issue(uint32_t raw_inst,
             uint32_t pc,
             uint32_t rs1_val,
             uint32_t rs2_val) override;

  bool has_response() const override;
  uint32_t read_response() override;

  uint32_t mem_load32(uint32_t addr) override;
  void mem_store32(uint32_t addr, uint32_t data) override;

private:
  AccelMemBridge& ab_;

  bool busy_ = false;
  bool has_resp_ = false;
  uint32_t resp_ = 0;

  uint32_t base_ = 0;
  uint32_t len_ = 0;
  uint32_t idx_ = 0;
  uint32_t sum_ = 0;
  bool waiting_load_ = false;
};

