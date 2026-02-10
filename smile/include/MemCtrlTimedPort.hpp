// **********************************************************************
// smile/include/MemCtrlTimedPort.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 9 2026
/*
Tiny fake memory controller that sits in front of existing MemoryPort.
Forces each read/write to take fixed number of cycles before a response appears.
Also supports immediate read32/write32 passthrough for loader/debugger/accel use
(i.e., if you call read32/write32 directly, it just forwards to the backing port with no delay). 
*/

#pragma once

#include "Tile1.hpp"

class MemCtrlTimedPort : public MemoryPort {
public:
  MemCtrlTimedPort(MemoryPort* backing, int latency_cycles); // constructor takes pointer to backing port and fixed latency in cycles

  void set_latency(int v);

  // Immediate compatibility path (loader/debugger/accels).
  uint32_t read32(uint32_t addr) override;
  void write32(uint32_t addr, uint32_t value) override;

  // Timed request/response path.
  void cycle() override;
  bool can_request() const override;
  void request_read32(uint32_t addr) override;
  void request_write32(uint32_t addr, uint32_t value) override;
  bool resp_valid() const override;
  uint32_t resp_data() const override;
  void resp_consume() override;

private:
  MemoryPort* backing_ = nullptr;
  int latency_ = 0;
  bool in_flight_ = false;
  bool is_write_ = false;
  uint32_t req_addr_ = 0;
  uint32_t req_wdata_ = 0;
  int cnt_ = 0;
  bool resp_valid_ = false;
  uint32_t resp_data_ = 0;
};
