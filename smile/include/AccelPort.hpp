// **********************************************************************
// smile/include/AccelPort.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Nov 17 2025 
/*
Abstract API (not a simulatable object), just a protocol, for talking as co-processor/accelerator.
Lightweight accelerator port (sim how CPU talks to an accelerator).  Implementation
can back this with any Cascade component that exposes custom instruction issue/response.

A concrete accelerator will inherit from this class and implement the issue() method
(and optionally tick()/has_response()/read_response() methods for multi-cycle accelerators).
*/

#pragma once

#include <cstdint>

class AccelPort {
public:
  // Interface-level error codes (v1 contract: docs/accel_port.md).
  static constexpr uint32_t ACCEL_E_UNSUPPORTED = 1u;
  static constexpr uint32_t ACCEL_E_BUSY        = 2u; // reserved
  static constexpr uint32_t ACCEL_E_BADARG      = 3u; // reserved

  virtual ~AccelPort() = default;

  // Issue a single accelerator request corresponding to a custom instruction.
  // In v1 blocking mode, Tile1 calls issue() at most once per CUSTOM-0 issue.
  // Tile1 then waits (if needed) until has_response() reports completion.
  // raw_inst : the 32-bit instruction word
  // pc       : PC at which it was issued
  // rs1_val  : value of rs1 at issue
  // rs2_val  : value of rs2 at issue
  virtual void issue(uint32_t raw_inst,
                     uint32_t pc,
                     uint32_t rs1_val,
                     uint32_t rs2_val) = 0;

  // Optional: model multi-cycle behavior.
  // For now you can leave this empty in stub implementations.
  virtual void tick() {}

  // Optional: true if the accelerator cannot accept a new issue() this cycle.
  // v1 Tile1 does not consult busy(); it uses issue-once + stall on has_response().
  virtual bool busy() const { return false; }

  // Optional response side, if/when you want rd results.
  // Contract: once a response is ready, has_response() must remain true until
  // read_response() consumes one pending response.
  virtual bool has_response() const { return false; }
  // Contract for implementations with responses: consumes exactly one response.
  // After consume, has_response() becomes false unless another response is queued.
  // Default stub returns 0 and has no queued-response state.
  virtual uint32_t read_response() { return 0; }

  // --------------------------------------------------------------------
  // Memory access API (RoCC-style, simplified as blocking operations).
  //
  // These allow an accelerator to behave like a first-class memory client,
  // similar to a RoCC accelerator talking to L1. In this first version,
  // the calls are blocking and return immediately, but conceptually they
  // wrap a request/response handshake against the memory system.
  //
  // Accelerators that need memory should override these and route the
  // calls to an attached MemoryPort (e.g., DRAM or a future cache).
  // --------------------------------------------------------------------
  virtual uint32_t mem_load32(uint32_t /*addr*/) {
    return 0;
  }

  virtual void mem_store32(uint32_t /*addr*/, uint32_t /*data*/) {
  }
};
