// **********************************************************************
// smesh/tests/unit/dma/tb_dma_writer_pool.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 29 2026
/* Signed max pooling, window boundaries, zero padding, and stalled writes. */

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>
#include "DmaWriter.hpp"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace {

struct ExpectedBeat {
  smem::MemReq req{};
  unsigned accepted_rows = 0;
};

class Driver : public Component {
  DECLARE_COMPONENT(Driver);
 public:
  Driver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, req_val);
  Output(smesh::StWriterReq, req_bits);
  Input(bit, req_rdy);
  FifoInput(smem::MemReq, mem_req);
  void drive();
  void observe();
  void reset();
  bool check() const;
  unsigned accepted() const { return unsigned(*index_Q_); }

 private:
  void add(int value, bool pool, bool store, bool zero = false, unsigned bytes = smesh::kDim);
  void expect(std::initializer_list<int> pattern, unsigned bytes = smesh::kDim);
  Output(u16, index_Q_);
  Register(u16, index_D_);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
  std::vector<smesh::StWriterReq> rows_;
  std::vector<ExpectedBeat> expected_;
  unsigned received_ = 0;
  unsigned stalls_ = 0;
  bool good_ = true;
};

void Driver::add(int value, bool pool, bool store, bool zero, unsigned bytes) {
  smesh::StWriterReq row{};
  row.issue.vaddr = 0x80001000ull + 0x100 * rows_.size();
  row.issue.cmd_id = rows_.size();
  row.issue.pool_en = bit(pool);
  row.issue.store_en = bit(store);
  row.len_bytes = bytes;
  row.data_is_all_zeros = bit(zero);
  row.data_is_full_width = bit(bytes > smesh::kDim);
  // Alternating signs ensure comparison is independently checked per element.
  for (unsigned lane = 0; lane < bytes; ++lane)
    row.data[lane] = static_cast<std::uint8_t>(lane % 2 ? -value : value);
  rows_.push_back(row);
}

void Driver::expect(std::initializer_list<int> pattern, unsigned bytes) {
  const auto& row = rows_.back();
  for (unsigned offset = 0; offset < bytes; offset += smesh::kMemBeatBytes) {
    ExpectedBeat beat{};
    beat.accepted_rows = rows_.size();
    beat.req.addr = std::uint64_t(row.issue.vaddr) + offset;
    beat.req.id = row.issue.cmd_id;
    beat.req.write = 1;
    const auto count = std::min<unsigned>(smesh::kMemBeatBytes, bytes - offset);
    beat.req.size = count;
    std::uint64_t data = 0;
    for (unsigned byte = 0; byte < count; ++byte) {
      const int element = *(pattern.begin() + (offset + byte) % pattern.size());
      data |= std::uint64_t(static_cast<std::uint8_t>(element)) << (8 * byte);
    }
    beat.req.wdata = data;
    expected_.push_back(beat);
  }
}

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  index_Q_ <= index_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(drive).reads(index_Q_, cycle_Q_).writes(req_val, req_bits);
  UPDATE(observe).reads(index_Q_, cycle_Q_, req_val, req_rdy, mem_req)
                 .writes(index_D_, cycle_D_);
  // First window exercises all-negative channels and signed extrema.
  add(-128, false, false);
  add(-7, true, false);
  add(-2, true, true);
  expect({-2, 7});
  add(-9, false, true); expect({-9, 9}); // one-position window replaces old maxima
  add(-6, false, false);
  add(99, true, true, true); expect({0, 6}); // zero padding keeps positive saved values
  add(99, false, false, true);
  add(-5, true, true); expect({0, 5});
  add(-3, false, true, false, 3); expect({-3, 3}, 3); // partial ordinary row
  add(-11, false, true, false, smesh::kDim * sizeof(smesh::Acc));
  expect({-11, 11}, smesh::kDim * sizeof(smesh::Acc)); // ordinary multi-beat write
  add(-127, false, false);
  add(127, true, true); expect({127}); // new window, signed extrema
}

void Driver::drive() {
  const auto index = unsigned(*index_Q_);
  req_val = bit(Sim::state != Sim::SimResetting && index < rows_.size() && unsigned(*cycle_Q_) % 7 != 3);
  req_bits = index < rows_.size() ? rows_[index] : smesh::StWriterReq{};
}

void Driver::observe() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = unsigned(*cycle_Q_) + 1;
  const auto accepted_rows = unsigned(*index_Q_) + unsigned(req_val == 1 && req_rdy == 1);
  if (accepted_rows != unsigned(*index_Q_)) index_D_ = accepted_rows;
  if (req_val == 1 && req_rdy == 0) ++stalls_;
  // Fill the output FIFO first; later alternate draining and stalls.
  if (unsigned(*cycle_Q_) < 35 || unsigned(*cycle_Q_) % 5 < 2 || mem_req.empty()) return;
  const auto got = mem_req.pop();
  if (received_ >= expected_.size()) { good_ = false; return; }
  const auto& expected = expected_[received_++];
  if (accepted_rows < expected.accepted_rows || got.addr != expected.req.addr ||
      got.id != expected.req.id || got.size != expected.req.size ||
      got.write != 1 || got.wdata != expected.req.wdata) {
    std::printf("beat %u mismatch: addr=%llx data=%llx expected=%llx\n", received_,
                (unsigned long long)got.addr, (unsigned long long)got.wdata,
                (unsigned long long)expected.req.wdata);
    good_ = false;
  }
}

void Driver::reset() {
  index_D_.reset(0);
  cycle_D_.reset(0);
  req_val.reset(0);
  req_bits.reset(smesh::StWriterReq{});
  received_ = stalls_ = 0;
  good_ = true;
}

bool Driver::check() const {
  return good_ && accepted() == rows_.size() && received_ == expected_.size() && stalls_ > 0;
}
} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);
  Driver driver("Driver");
  smesh::DmaWriter writer("DmaWriter");
  writer.req_val << driver.req_val;
  writer.req_bits << driver.req_bits;
  driver.req_rdy << writer.req_rdy;
  driver.mem_req << writer.mem_req;
  writer.mem_req.setSize(2);
  Clock clk;
  driver.clk << clk;
  writer.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (unsigned cycle = 0; cycle < 10 && driver.accepted() < 1; ++cycle) Sim::run();
  // Reset with an unfinished window, then run the complete test twice.
  bool pass = true;
  for (unsigned trial = 0; trial < 2; ++trial) {
    Sim::reset();
    for (unsigned cycle = 0; cycle < 160; ++cycle) Sim::run();
    pass = driver.check() && pass;
  }
  std::printf("[DMA_WRITER_POOL] %s DIM=%zu beat=%zu\n", pass ? "PASS" : "FAIL", smesh::kDim, smesh::kMemBeatBytes);
  descore::flushLog();
  return pass ? 0 : 1;
}
