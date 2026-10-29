// **********************************************************************
// smesh/tests/unit/dma/tb_dma_write_queues.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 29 2026
/* Check registered queue visibility, hold, dequeue, and refill cycle by cycle. */

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "DmaIssueQueues.hpp"

#include <cstdio>
#include <deque>

namespace {
class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, enq_val);
  Output(smesh::DmaWriteReq, enq_bits);
  Input(bit, enq_rdy);
  Input(bit, deq_val);
  Input(smesh::DmaWriteReq, deq_bits);
  Output(bit, deq_rdy);
  void drive();
  void observe();
  void reset();
  bool passed() const { return good_ && expected_.empty() && popped_ > 10 && held_ > 5; }
 private:
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  std::deque<smesh::DmaWriteReq> expected_;
  bool good_ = true;
  unsigned popped_ = 0;
  unsigned held_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle_Q_ <= cycle_D_;
  UPDATE(drive).reads(cycle_Q_).writes(enq_val, enq_bits, deq_rdy);
  UPDATE(observe).reads(cycle_Q_, enq_val, enq_bits, enq_rdy, deq_val, deq_bits, deq_rdy)
                 .writes(cycle_D_);
}

void Driver::drive() {
  const auto cycle = unsigned(*cycle_Q_);
  enq_val = bit(Sim::state != Sim::SimResetting && cycle < 70 && cycle % 7 != 0);
  deq_rdy = bit(cycle % 5 >= 2);
  smesh::DmaWriteReq req{};
  req.vaddr = 0x80000000ull + cycle;
  req.cmd_id = cycle;
  req.pool_en = bit(cycle % 2);
  req.store_en = bit(cycle % 3 == 0);
  enq_bits = req;
}

void Driver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = unsigned(*cycle_Q_) + 1;
  const bool valid = !expected_.empty();
  // Compare this cycle's outputs to transfers from previous cycles only.
  good_ = good_ && (deq_val == 1) == valid && (enq_rdy == 1) == !valid;
  if (valid) {
    const auto& expected = expected_.front();
    good_ = good_ && deq_bits->vaddr == expected.vaddr &&
        deq_bits->cmd_id == expected.cmd_id && deq_bits->pool_en == expected.pool_en &&
        deq_bits->store_en == expected.store_en;
    if (deq_rdy == 0) ++held_;
  }
  if (deq_val == 1 && deq_rdy == 1) {
    if (expected_.empty()) good_ = false;
    else expected_.pop_front();
    ++popped_;
  }
  if (enq_val == 1 && enq_rdy == 1) expected_.push_back(*enq_bits);
}

void Driver::reset() {
  cycle_D_.reset(0);
  enq_val.reset(0);
  enq_bits.reset(smesh::DmaWriteReq{});
  deq_rdy.reset(0);
  expected_.clear();
  good_ = true;
  popped_ = held_ = 0;
}

template<class Queue>
void connect(Queue& queue, Driver& driver, Clock& clk) {
  queue.clk << clk;
  driver.clk << clk;
  queue.enq_val << driver.enq_val;
  queue.enq_bits << driver.enq_bits;
  queue.deq_rdy << driver.deq_rdy;
  driver.enq_rdy << queue.enq_rdy;
  driver.deq_val << queue.deq_val;
  driver.deq_bits << queue.deq_bits;
}
} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  smesh::DmaWriteNormQueue norm("Norm");
  smesh::DmaWriteScaleQueue scale("Scale");
  smesh::DmaWriteIssueQueue issue("Issue");
  Driver norm_driver("NormDriver"), scale_driver("ScaleDriver"), issue_driver("IssueDriver");
  Clock clk;
  connect(norm, norm_driver, clk);
  connect(scale, scale_driver, clk);
  connect(issue, issue_driver, clk);
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  bool pass = true;
  for (unsigned trial = 0; trial < 2; ++trial) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 100; ++cycle) Sim::run();
    pass = norm_driver.passed() && scale_driver.passed() && issue_driver.passed() && pass;
  }
  std::printf("[DMA_WRITE_QUEUES] %s\n", pass ? "PASS" : "FAIL");
  descore::flushLog();
  return pass ? 0 : 1;
}
