// **********************************************************************
// smicro/src/AccelArraySumSoc.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 17 2026
/*
AccelArraySumSoc: a toy accelerator model for smicro that implements the
shared AccelPort (CUSTOM-0) contract using the real SoC memory path.

Purpose:
- Acts like a small “device” attached to Tile1: Tile1 issues CUSTOM-0 (funct3=0)
  with rs1=base address and rs2=length, then blocks until the accelerator responds.
- While the core is stalled, this accelerator advances cycle-by-cycle via tick().

How it works:
- issue(): validates verb/alignment and captures (base,len), then marks the op busy.
- tick(): repeatedly schedules one 32-bit load at a time using AccelMemBridge
  (AccelMemBridge emits smem::MemReq/smem::MemResp traffic into MemCtrl), accumulates a
  uint32_t sum, and finally publishes a sticky response (sum) to the core.
- Completion is reported through has_response()/read_response() per AccelPort v1.

Mini topology (where this block sits in smicro):

        Tile1 (exec CUSTOM-0)
               |
               |  AccelPort::issue()/has_response()
               v
        +------------------+
        | AccelArraySumSoc |
        |  (tick() FSM)    |
        +------------------+
               |
               |  host API: start_load32()/resp_*()
               v
        +------------------+
        |  AccelMemBridge  |
        | (smem::MemReq/smem::MemResp) |
        +------------------+
               |
               |  MemCtrl protocol FIFOs
               v
        +------------------+     +--------+
        |      MemCtrl     |<--->|  Dram  |
        +------------------+     +--------+

Notes:
- Tile1 stalls while AccelArraySumSoc is busy; the accelerator progresses via tick().
- Memory traffic is via MemCtrl timing (through AccelMemBridge), not Tile1's private DRAM shim.

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
  // AccelPort interface: issue() captures req (from Tile1_exec.cpp) and starts accel FSM… 
  void issue(uint32_t raw_inst,
             uint32_t pc,
             uint32_t rs1_val,
             uint32_t rs2_val) override;
  // …has_response()/read_response() publish the result when ready.
  bool     has_response() const override;
  uint32_t read_response() override;

  uint32_t mem_load32(uint32_t addr) override;
  void     mem_store32(uint32_t addr, uint32_t data) override;

private:
  AccelMemBridge& ab_;

  bool busy_     = false;
  bool has_resp_ = false;
  uint32_t resp_ = 0;

  uint32_t base_     = 0;
  uint32_t len_      = 0;
  uint32_t idx_      = 0;
  uint32_t sum_      = 0;
  bool waiting_load_ = false;
};

