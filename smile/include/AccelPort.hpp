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
  virtual ~AccelPort() = default;

  // Issue a single accelerator request corresponding to a custom instruction.
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

  // Optional: response side, if/when you want rd results later.
  virtual bool has_response() const { return false; }
  virtual uint32_t read_response() { return 0; }
};