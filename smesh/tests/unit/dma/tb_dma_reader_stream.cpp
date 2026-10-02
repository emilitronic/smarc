// **********************************************************************
// smesh/tests/unit/dma/tb_dma_reader_stream.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 1 2026

// Check that a multi-row DMA read streams complete rows and holds them under backpressure.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "DmaReader.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint64_t kBase = 0x80001000;
constexpr unsigned kCols = 3 * smesh::kDim + 2;
constexpr unsigned kRows = 4;

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::DmaReadReq, req_out);
  Input(bit, resp_val);
  Input(smesh::DmaReadResp, resp_bits);
  Output(bit, resp_rdy);
  Input(u16, mem_delivered);
  Output(u16, cycle);

  void updateRequest();
  void updateReady();
  void updateCheck();
  void reset();
  bool passed() const;

 private:
  Register(u16, cycle_D_);
  bool sent_ = false;
  bool saw_early_row_ = false;
  bool saw_stall_ = false;
  bool data_ok_ = true;
  unsigned received_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  cycle <= cycle_D_;
  UPDATE(updateRequest).writes(req_out);
  UPDATE(updateReady).reads(cycle).writes(resp_rdy);
  UPDATE(updateCheck).reads(cycle, resp_val, resp_bits, resp_rdy, mem_delivered)
      .writes(cycle_D_);
}

void Driver::updateRequest() {
  if (sent_ || req_out.full()) return;
  smesh::DmaReadReq req{};
  req.vaddr = kBase;
  req.laddr = smesh::makeSpAddr(2);
  req.cols = u16(kCols);
  req.block_stride = 1;
  req.scale = 0x3f800000u;
  req.cmd_id = 7;
  req_out.push(req);
  sent_ = true;
}

void Driver::updateReady() {
  resp_rdy = bit(cycle >= 20);
}

void Driver::updateCheck() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = u16(static_cast<std::uint16_t>(*cycle) + 1);
  if (resp_val == 0) return;

  if (received_ == 0 && mem_delivered < 2) saw_early_row_ = true;
  if (resp_rdy == 0) saw_stall_ = true;
  const auto& row = *resp_bits;
  const unsigned cols = std::min<unsigned>(smesh::kDim, kCols - received_ * smesh::kDim);
  data_ok_ &= received_ < kRows && row.laddr.full_sp_addr() == 2 + received_ &&
              static_cast<unsigned>(row.len) == cols &&
              static_cast<std::uint32_t>(row.mask) == (1u << cols) - 1u &&
              static_cast<unsigned>(row.bytes_read) == cols &&
              row.cmd_id == 7 && row.last == 1 && row.has_acc_bitwidth == 0;
  for (unsigned lane = 0; lane < cols; ++lane) {
    data_ok_ &= row.data[lane] == 0x20 + received_ * smesh::kDim + lane;
  }
  if (resp_rdy == 1) ++received_;
}

void Driver::reset() {
  cycle_D_.reset(0);
  resp_rdy.reset(0);
  sent_ = false;
  saw_early_row_ = false;
  saw_stall_ = false;
  data_ok_ = true;
  received_ = 0;
}

bool Driver::passed() const {
  return sent_ && saw_early_row_ && saw_stall_ && data_ok_ && received_ == kRows;
}

class Memory : public Component {
  DECLARE_COMPONENT(Memory);

 public:
  Memory(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoInput(smem::MemReq, req_in);
  FifoOutput(smem::MemResp, resp_out);
  Input(u16, cycle);
  Output(u16, delivered);

  void updateResponse();
  void updateAccept();
  void reset();
  bool passed() const;

 private:
  struct Pending {
    bit valid = 0;
    smem::MemReq req{};
    u16 due = 0;
  };

  Output(Pending, pending_Q_);
  Register(Pending, pending_D_);
  Output(bit, response_sent_);
  Output(u16, requested_Q_);
  Register(u16, requested_D_);
  Register(u16, delivered_D_);
  bool requests_ok_ = true;
};

Memory::Memory(std::string /*name*/, IMPL_CTOR) {
  pending_Q_ <= pending_D_;
  requested_Q_ <= requested_D_;
  delivered <= delivered_D_;
  UPDATE(updateResponse).reads(pending_Q_, cycle, resp_out, delivered)
      .writes(resp_out, response_sent_, delivered_D_);
  UPDATE(updateAccept).reads(pending_Q_, response_sent_, req_in, cycle, requested_Q_)
      .writes(pending_D_, requested_D_);
}

void Memory::updateResponse() {
  response_sent_ = 0;
  const auto pending = *pending_Q_;
  if (pending.valid == 0 || cycle < pending.due || resp_out.full()) return;
  const auto& req = pending.req;
  smem::MemResp resp{};
  resp.id = req.id;
  for (unsigned byte = 0; byte < static_cast<std::uint16_t>(req.size); ++byte) {
    const auto offset = static_cast<std::uint64_t>(req.addr) - kBase + byte;
    resp.rdata |= std::uint64_t{0x20 + offset} << (8 * byte);
  }
  resp_out.push(resp);
  response_sent_ = 1;
  delivered_D_ = u16(static_cast<std::uint16_t>(*delivered) + 1);
}

void Memory::updateAccept() {
  const auto pending = *pending_Q_;
  if (pending.valid == 1 && response_sent_ == 0) return;
  if (req_in.empty()) {
    if (pending.valid == 1) pending_D_ = Pending{};
    return;
  }
  const auto req = req_in.pop();
  const unsigned index = static_cast<std::uint16_t>(*requested_Q_);
  const auto offset = index * smesh::kMemBeatBytes;
  requests_ok_ &= req.write == 0 && req.id == 7 &&
                  static_cast<std::uint64_t>(req.addr) == kBase + offset &&
                  static_cast<std::uint16_t>(req.size) ==
                      std::min<unsigned>(smesh::kMemBeatBytes, kCols - offset);
  Pending next{};
  next.valid = 1;
  next.req = req;
  next.due = u16(static_cast<std::uint16_t>(*cycle) + (index == 0 ? 2 : 10));
  pending_D_ = next;
  requested_D_ = u16(index + 1);
}

void Memory::reset() {
  pending_D_.reset(Pending{});
  requested_D_.reset(0);
  delivered_D_.reset(0);
  response_sent_.reset(0);
  requests_ok_ = true;
}

bool Memory::passed() const {
  const auto expected = (kCols + smesh::kMemBeatBytes - 1) / smesh::kMemBeatBytes;
  return requests_ok_ && requested_Q_ == expected && delivered == expected;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  Driver driver("Driver");
  Memory memory("Memory");
  smesh::DmaReader reader("DmaReader");
  reader.req_in << driver.req_out;
  memory.req_in << reader.mem_req;
  reader.mem_resp << memory.resp_out;
  driver.resp_val << reader.resp_val;
  driver.resp_bits << reader.resp_bits;
  reader.resp_rdy << driver.resp_rdy;
  driver.mem_delivered << memory.delivered;
  memory.cycle << driver.cycle;

  Clock clk;
  driver.clk << clk;
  memory.clk << clk;
  reader.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int step = 0; step < 100; ++step) Sim::run();
  const bool pass = driver.passed() && memory.passed();
  std::printf("[DMA_READER_STREAM] %s beat=%zu\n", pass ? "PASS" : "FAIL", smesh::kMemBeatBytes);
  return pass ? 0 : 1;
}
