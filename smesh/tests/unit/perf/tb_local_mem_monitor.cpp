// **********************************************************************
// smesh/tests/unit/perf/tb_local_mem_monitor.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 23 2026
#include "tb_rs_to_mem_monitor.hpp"

#include <cstdio>

namespace {

using namespace smesh;

// Hold the same handshakes for several cycles to check per-cycle counting.
class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, val);
  Output(bit, rdy);
  Output(bit, resp_rdy);
  bool active = false;
  bool blocked = false;
  void update() {
    val = bit(active);
    rdy = bit(!blocked);
    resp_rdy = bit(blocked);
  }
  void reset() {
    active = false;
    blocked = false;
  }
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(val, rdy, resp_rdy);
}

bool matches(const tb::LocalMemMonitor& monitor, std::size_t peak,
             std::size_t reads, std::size_t stalls, bool concurrent) {
  const bool ok = monitor.maxConcurrentSpadBanks() == peak &&
      monitor.exAccumReads() == reads && monitor.storeAccumReads() == reads &&
      monitor.exAccumResponseStallCycles() == stalls &&
      monitor.concurrentAccumReads() == concurrent;
  if (!ok) {
    std::printf("  expected peak=%zu reads=%zu stalls=%zu concurrent=%u; observed peak=%zu ex=%zu store=%zu stalls=%zu concurrent=%u\n",
                peak, reads, stalls, unsigned(concurrent), monitor.maxConcurrentSpadBanks(),
                monitor.exAccumReads(), monitor.storeAccumReads(),
                monitor.exAccumResponseStallCycles(), unsigned(monitor.concurrentAccumReads()));
  }
  return ok;
}

} // namespace

int main() {
  static_assert(kSpBanks >= 2 && kAccBanks >= 2, "Test requires two banks");
  Clock clk;
  Driver driver("Driver");
  smesh::tb::LocalMemMonitor monitor("LocalMemMonitor");
  driver.clk << clk;
  monitor.clk << clk;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (bank < 2) monitor.spad_read_req_val[bank] << driver.val;
    else monitor.spad_read_req_val[bank].wireToConst(bit(0));
    monitor.spad_read_req_rdy[bank] << driver.rdy;
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (bank < 2) monitor.accum_read_req_val[bank] << driver.val;
    else monitor.accum_read_req_val[bank].wireToConst(bit(0));
    monitor.accum_read_req_rdy[bank] << driver.rdy;
    AccumBankReadReq req{};
    req.from_dma = bit(bank == 1);
    monitor.accum_read_req_bits[bank].wireToConst(req);
    if (bank == 0) monitor.accum_read_resp_val[bank] << driver.val;
    else monitor.accum_read_resp_val[bank].wireToConst(bit(0));
    monitor.accum_read_resp_rdy[bank] << driver.resp_rdy;
    AccumReadResp resp{};
    resp.from_dma = 0;
    monitor.accum_read_resp_bits[bank].wireToConst(resp);
  }
  clk.generateClock();
  Sim::init();
  Sim::reset();
  bool ok = matches(monitor, 0, 0, 0, false);

  driver.active = true;
  for (int cycle = 1; cycle <= 4; ++cycle) {
    Sim::run();
    ok &= matches(monitor, 2, cycle, cycle, true);
  }
  // Requests stay valid but cannot transfer; the response is now accepted.
  driver.blocked = true;
  for (int cycle = 0; cycle < 3; ++cycle) Sim::run();
  ok &= matches(monitor, 2, 4, 4, true);
  driver.active = false;
  for (int cycle = 0; cycle < 2; ++cycle) Sim::run();
  ok &= matches(monitor, 2, 4, 4, true);

  Sim::reset();
  ok &= matches(monitor, 0, 0, 0, false);
  Sim::run();
  ok &= matches(monitor, 0, 0, 0, false);
  std::printf("[LOCAL_MEM_MONITOR] %s repeated cycles, handshakes, stalls, reset\n",
              ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
