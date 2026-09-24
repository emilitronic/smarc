// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl_dma_req.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrlDmaReq.hpp"

#include <array>
#include <cstdio>

namespace {

struct Case {
  std::uint64_t vaddr;
  smesh::SmeshLocalAddr laddr;
  std::uint32_t cols;
  std::uint32_t rows;
  std::uint32_t actual_rows;
  std::uint64_t stride;
  bool all_zeros;
  bool shrink;
  std::uint32_t bytes;
  std::uint16_t repeats;
  bool acc_width;
};

const std::array<Case, 5> kCases{{
    {0x1020, smesh::makeSpAddr(7), 4, 3, 3, 16, false, false, 12, 0, false},
    {0x2000, smesh::makeAccAddr(8), 4, 2, 2, 16, false, false, 32, 0, true},
    {0x2010, smesh::makeAccAddr(9), 4, 2, 2, 16, false, true, 8, 0, false},
    {0x3000, smesh::makeSpAddr(4), 4, 3, 1, 0, false, false, 4, 2, false},
    {0, smesh::makeSpAddr(5), 4, 3, 3, 0, true, false, 12, 0, false},
}};

class ReqDriver : public Component {
  DECLARE_COMPONENT(ReqDriver);

 public:
  ReqDriver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(u64, current_vaddr);
  Output(smesh::SmeshLocalAddr, current_localaddr);
  Output(u32, cols);
  Output(u32, rows);
  Output(u32, actual_rows_read);
  Output(u64, stride);
  Output(bit, all_zeros);
  Output(u32, scale);
  Output(bit, shrink);
  Output(u16, block_stride);
  Output(u8, pixel_repeat);
  Output(u16, cmd_id);
  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class ReqMonitor : public Component {
  DECLARE_COMPONENT(ReqMonitor);

 public:
  ReqMonitor(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, has_acc_bitwidth);
  Input(u32, bytes_to_read);
  Input(smesh::DmaReadReq, req_bits);
  void update();
  void reset();
  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

ReqDriver::ReqDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(current_vaddr, current_localaddr, cols, rows,
              actual_rows_read, stride, all_zeros, scale)
      .writes(shrink, block_stride, pixel_repeat, cmd_id);
}

void ReqDriver::update() {
  const auto& c = kCases[cycle_ < kCases.size() ? cycle_ : kCases.size() - 1];
  current_vaddr = c.vaddr;
  current_localaddr = c.laddr;
  cols = c.cols;
  rows = c.rows;
  actual_rows_read = c.actual_rows;
  stride = c.stride;
  all_zeros = bit(c.all_zeros);
  scale = 0x12345678u;
  shrink = bit(c.shrink);
  block_stride = 7;
  pixel_repeat = 1;
  cmd_id = static_cast<std::uint16_t>(cycle_);
  ++cycle_;
}

void ReqDriver::reset() {
  cycle_ = 0;
  current_vaddr.reset(0);
  current_localaddr.reset(smesh::SmeshLocalAddr{});
  cols.reset(0);
  rows.reset(0);
  actual_rows_read.reset(0);
  stride.reset(0);
  all_zeros.reset(0);
  scale.reset(0);
  shrink.reset(0);
  block_stride.reset(0);
  pixel_repeat.reset(0);
  cmd_id.reset(0);
}

ReqMonitor::ReqMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(has_acc_bitwidth, bytes_to_read, req_bits);
}

void ReqMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  const auto& c = kCases[cycle_];
  const auto req = *req_bits;
  passed_ &= (has_acc_bitwidth == 1) == c.acc_width;
  passed_ &= static_cast<std::uint32_t>(bytes_to_read) == c.bytes;
  passed_ &= static_cast<std::uint64_t>(req.vaddr) == c.vaddr;
  passed_ &= req.laddr.raw == c.laddr.raw;
  passed_ &= static_cast<std::uint16_t>(req.cols) == c.cols;
  passed_ &= static_cast<std::uint16_t>(req.repeats) == c.repeats;
  passed_ &= static_cast<std::uint16_t>(req.block_stride) == 7;
  passed_ &= static_cast<std::uint32_t>(req.scale) == 0x12345678u;
  passed_ &= (req.has_acc_bitwidth == 1) == c.acc_width;
  passed_ &= (req.all_zeros == 1) == c.all_zeros;
  passed_ &= static_cast<std::uint8_t>(req.pixel_repeats) == 1;
  passed_ &= static_cast<std::uint16_t>(req.cmd_id) == cycle_;

  std::printf("[c%u] addr=%08x cols=%u repeats=%u acc=%u zero=%u bytes=%u id=%u\n",
              cycle_, req.laddr.raw, static_cast<unsigned>(req.cols),
              static_cast<unsigned>(req.repeats),
              static_cast<unsigned>(req.has_acc_bitwidth == 1),
              static_cast<unsigned>(req.all_zeros == 1),
              static_cast<unsigned>(bytes_to_read),
              static_cast<unsigned>(req.cmd_id));
  done_ = cycle_ + 1 == kCases.size();
  ++cycle_;
}

void ReqMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LdCtrlDmaReq request("LoadRequest");
  ReqDriver driver("Driver");
  ReqMonitor monitor("Monitor");
  request.current_vaddr << driver.current_vaddr;
  request.current_localaddr << driver.current_localaddr;
  request.cols << driver.cols;
  request.rows << driver.rows;
  request.actual_rows_read << driver.actual_rows_read;
  request.stride << driver.stride;
  request.all_zeros << driver.all_zeros;
  request.scale << driver.scale;
  request.shrink << driver.shrink;
  request.block_stride << driver.block_stride;
  request.pixel_repeat << driver.pixel_repeat;
  request.cmd_id << driver.cmd_id;
  monitor.has_acc_bitwidth << request.has_acc_bitwidth;
  monitor.bytes_to_read << request.bytes_to_read;
  monitor.req_bits << request.req_bits;

  Clock clk;
  request.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL_DMA_REQ] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
