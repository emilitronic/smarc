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
  Output(bit, resp_rdy);
  FifoInput(DmaReadCompletion, spad_dma_resp);
  FifoInput(DmaReadCompletion, accum_dma_resp);
  bool active = false;
  bool blocked = false;
  void update() {
    if (!spad_dma_resp.empty()) spad_dma_resp.pop();
    if (!accum_dma_resp.empty()) accum_dma_resp.pop();
    val = bit(active);
    resp_rdy = bit(!blocked);
  }
  void reset() {
    active = false;
    blocked = false;
  }
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(spad_dma_resp, accum_dma_resp).writes(val, resp_rdy);
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
  Spad spad("Spad");
  Accum accum("Accum");
  smesh::tb::LocalMemMonitor monitor;
  driver.clk << clk;
  spad.clk << clk;
  accum.clk << clk;
  driver.spad_dma_resp << spad.dma_resp;
  driver.accum_dma_resp << accum.dma_resp;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    if (bank < 2) spad.read_req_val_bnk[bank] << driver.val;
    else spad.read_req_val_bnk[bank].wireToConst(bit(0));
    spad.read_req_bits_bnk[bank].wireToConst(SpadBankReadReq{});
    spad.read_resp_rdy_bnk[bank] << driver.resp_rdy;
    spad.write_val_bnk[bank].wireToConst(bit(0));
    spad.write_bits_bnk[bank].wireToConst(DmaReadResp{});
  }
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    if (bank < 2) accum.read_req_val_bnk[bank] << driver.val;
    else accum.read_req_val_bnk[bank].wireToConst(bit(0));
    AccumBankReadReq req{};
    req.from_dma = bit(bank == 1);
    accum.read_req_bits_bnk[bank].wireToConst(req);
    accum.read_resp_rdy_bnk[bank] << driver.resp_rdy;
    accum.write_val_bnk[bank].wireToConst(bit(0));
    accum.write_bits_bnk[bank].wireToConst(DmaReadResp{});
  }
  clk.generateClock();
  Sim::init();
  Sim::reset();
  monitor.reset();
  bool ok = matches(monitor, 0, 0, 0, false);

  driver.active = true;
  for (int cycle = 1; cycle <= 4; ++cycle) {
    Sim::run();
    monitor.sample(spad, accum);
    ok &= matches(monitor, 2, cycle, 0, true);
  }
  // Occupied response slots block new requests while their responses stall.
  driver.blocked = true;
  for (int cycle = 1; cycle <= 3; ++cycle) {
    Sim::run();
    monitor.sample(spad, accum);
    ok &= matches(monitor, 2, 4, cycle, true);
  }
  driver.active = false;
  driver.blocked = false;
  for (int cycle = 0; cycle < 2; ++cycle) {
    Sim::run();
    monitor.sample(spad, accum);
  }
  ok &= matches(monitor, 2, 4, 3, true);

  Sim::reset();
  monitor.reset();
  ok &= matches(monitor, 0, 0, 0, false);
  Sim::run();
  monitor.sample(spad, accum);
  ok &= matches(monitor, 0, 0, 0, false);
  std::printf("[LOCAL_MEM_MONITOR] %s repeated cycles, handshakes, stalls, reset\n",
              ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
