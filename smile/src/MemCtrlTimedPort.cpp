// **********************************************************************
// smile/src/MemCtrlTimedPort.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 9 2026
// Timed single-outstanding MemoryPort wrapper.

#include "MemCtrlTimedPort.hpp"

MemCtrlTimedPort::MemCtrlTimedPort(MemoryPort* backing, int latency_cycles)
  : backing_(backing), latency_(latency_cycles) {
  assert_always(backing_ != nullptr, "MemCtrlTimedPort requires non-null backing port");
  if (latency_ < 0) latency_ = 0;
}

void MemCtrlTimedPort::set_latency(int v) {
  if (v < 0) v = 0;
  latency_ = v;
}

uint32_t MemCtrlTimedPort::read32(uint32_t addr) {
  return backing_->read32(addr);
}

void MemCtrlTimedPort::write32(uint32_t addr, uint32_t value) {
  backing_->write32(addr, value);
}

void MemCtrlTimedPort::cycle() {
  if (in_flight_ && cnt_ > 0) {
    --cnt_;
  }
  if (in_flight_ && cnt_ == 0 && !resp_valid_) {
    if (is_write_) {
      backing_->write32(req_addr_, req_wdata_);
      resp_data_ = 0;
    } else {
      resp_data_ = backing_->read32(req_addr_);
    }
    resp_valid_ = true;
    in_flight_ = false;
  }
}

bool MemCtrlTimedPort::can_request() const {
  return !in_flight_ && !resp_valid_;
}

void MemCtrlTimedPort::request_read32(uint32_t addr) {
  assert_always(can_request(), "MemCtrlTimedPort read request issued while busy");
  in_flight_ = true;
  is_write_ = false;
  req_addr_ = addr;
  cnt_ = latency_;
}

void MemCtrlTimedPort::request_write32(uint32_t addr, uint32_t value) {
  assert_always(can_request(), "MemCtrlTimedPort write request issued while busy");
  in_flight_ = true;
  is_write_ = true;
  req_addr_ = addr;
  req_wdata_ = value;
  cnt_ = latency_;
}

bool MemCtrlTimedPort::resp_valid() const {
  return resp_valid_;
}

uint32_t MemCtrlTimedPort::resp_data() const {
  return resp_data_;
}

void MemCtrlTimedPort::resp_consume() {
  resp_valid_ = false;
}
