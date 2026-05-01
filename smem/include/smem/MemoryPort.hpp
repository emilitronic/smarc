// **********************************************************************
// smem/include/smem/MemoryPort.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Apr 30 2026
/*
Lightweight software memory-port protocol used by Tile1, memory adapters,
debug tools, and accelerators. This is not a Cascade component by itself.
*/
#pragma once

#include <cstdint>

namespace smem {

class MemoryPort {
public:
  virtual          ~MemoryPort()                          = default;
  virtual uint32_t read32(uint32_t addr)                  = 0;
  virtual void     write32(uint32_t addr, uint32_t value) = 0;
  virtual void     cycle()                                = 0;
  virtual bool     can_request() const                    = 0;
  virtual void     request_read32(uint32_t addr)          = 0;
  virtual void     request_write32(uint32_t addr, uint32_t value) = 0;
  virtual bool     resp_valid() const                     = 0;
  virtual uint32_t resp_data() const                      = 0;
  virtual void     resp_consume()                         = 0;
};

} // namespace smem
