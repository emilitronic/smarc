// **********************************************************************
// smesh/tests/unit/dma/tb_dma_writer_beat4.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 30 2026
/*
 * Testbench for the DmaWriter component with 4 beats.
 */

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "DmaWriter.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

namespace {

class Driver : public Component {
  DECLARE_COMPONENT(Driver);

 public:
  Driver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit,                req_val);
  Output(smesh::StWriterReq, req_bits);
  Input(bit,                 req_rdy);
  FifoInput(smem::MemReq,    mem_req);

  void updateDrive();
  void updateObserve();
  void reset();
  bool check() const;

 private:
  Output(u8,   phase_Q_);
  Register(u8, phase_D_);
  Output(u8,   cycle_Q_);
  Register(u8, cycle_D_);
  std::array<smem::MemReq, 5> received_{};
  std::size_t received_count_ = 0;
};

Driver::Driver(std::string /*name*/, IMPL_CTOR) {
  phase_Q_ <= phase_D_;
  cycle_Q_ <= cycle_D_;
  UPDATE(updateDrive).reads(phase_Q_).writes(req_val, req_bits);
  UPDATE(updateObserve).reads(phase_Q_, cycle_Q_, req_val, req_rdy, mem_req)
                       .writes(phase_D_, cycle_D_);
}

void Driver::updateDrive() {
  const auto phase = static_cast<std::uint8_t>(*phase_Q_);
  req_val = bit(Sim::state != Sim::SimResetting && phase < 2);
  smesh::StWriterReq req{};
  req.issue.vaddr = phase == 0 ? 0x80001000ull : 0x80001100ull;
  req.issue.cmd_id = phase == 0 ? 7 : 8;
  req.len_bytes = phase == 0 ? 16 : 3;
  for (std::size_t byte = 0; byte < (phase == 0 ? 16u : 3u); ++byte) {
    req.data[byte] = static_cast<std::uint8_t>(phase == 0 ? byte + 1 : 0xa1 + byte);
  }
  req_bits = req;
}

void Driver::updateObserve() {
  if (Sim::state == Sim::SimResetting) return;
  cycle_D_ = u8(static_cast<std::uint8_t>(*cycle_Q_) + 1);
  if (req_val == 1 && req_rdy == 1) {
    phase_D_ = u8(static_cast<std::uint8_t>(*phase_Q_) + 1);
  }
  if (cycle_Q_ >= 5 && !mem_req.empty()) {
    const auto req = mem_req.pop();
    if (received_count_ < received_.size()) received_[received_count_] = req;
    ++received_count_;
  }
}

void Driver::reset() {
  phase_D_.reset(0);
  cycle_D_.reset(0);
  received_ = {};
  received_count_ = 0;
  req_val.reset(0);
  req_bits.reset(smesh::StWriterReq{});
}

bool Driver::check() const {
  if (static_cast<std::uint8_t>(*phase_Q_) != 2 || received_count_ != received_.size()) return false;
  for (std::size_t beat = 0; beat < received_.size(); ++beat) {
    const auto& req = received_[beat];
    const bool tail = beat == 4;
    const auto expected_addr = tail ? 0x80001100ull : 0x80001000ull + 4 * beat;
    const auto expected_size = tail ? 3u : 4u;
    const auto expected_id = tail ? 8u : 7u;
    std::uint64_t expected_data = 0;
    for (std::size_t byte = 0; byte < expected_size; ++byte) {
      expected_data |= static_cast<std::uint64_t>(tail ? 0xa1 + byte : 4 * beat + byte + 1)
                       << (8 * byte);
    }
    if (req.write != 1 || static_cast<std::uint64_t>(req.addr) != expected_addr ||
        static_cast<std::uint16_t>(req.size) != expected_size ||
        static_cast<std::uint16_t>(req.id) != expected_id ||
        static_cast<std::uint64_t>(req.wdata) != expected_data) return false;
  }
  return true;
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
  Clock clk;
  driver.clk << clk;
  writer.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 24; ++cycle) Sim::run();
  const bool pass = driver.check();
  std::printf("[DMA_WRITER_BEAT4] %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}
