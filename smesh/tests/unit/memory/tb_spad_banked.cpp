// **********************************************************************
// smesh/tests/unit/memory/tb_spad_banked.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 19 2026
// Focused checks for independent Spad banks and single-port arbitration.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Spad.hpp"

#include <array>
#include <cstdio>

namespace {

using namespace smesh;

constexpr std::uint32_t kRow0 = 3;
constexpr std::uint32_t kRow1 = 2;

DmaReadResp writeFor(std::size_t bank, std::uint32_t row, Elem base) {
  DmaReadResp write{};
  write.laddr = makeSpAddr(static_cast<std::uint32_t>(bank * kSpBankRows) + row);
  write.mask = static_cast<std::uint8_t>((1u << kDim) - 1u);
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    write.data[lane] = static_cast<std::uint8_t>(base + static_cast<Elem>(lane));
  }
  return write;
}

SpadBankReadReq readFor(std::uint32_t row, std::uint16_t cmd_id) {
  SpadBankReadReq read{};
  read.addr = row;
  read.len = kDim;
  read.cmd_id = cmd_id;
  return read;
}

bool rowMatches(const SpadReadResp& response, Elem base) {
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    if (response.data[lane] != base + static_cast<Elem>(lane)) {
      return false;
    }
  }
  return true;
}

class SpadBankedDriver : public Component {
  DECLARE_COMPONENT(SpadBankedDriver);

 public:
  SpadBankedDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  OutputArray(bit, write_val, kSpBanks);
  OutputArray(DmaReadResp, write_bits, kSpBanks);
  InputArray(bit, write_rdy, kSpBanks);
  OutputArray(bit, read_req_val, kSpBanks);
  OutputArray(SpadBankReadReq, read_req_bits, kSpBanks);
  InputArray(bit, read_req_rdy, kSpBanks);
  InputArray(bit, read_resp_val, kSpBanks);
  InputArray(SpadReadResp, read_resp_bits, kSpBanks);
  OutputArray(bit, read_resp_rdy, kSpBanks);
  FifoInput(DmaReadCompletion, dma_resp);

  void updateDrive();
  void updateCheck();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  Output(u8, phase_q_);
  Register(u8, phase_d_);
  bool done_ = false;
  bool passed_ = true;
};

SpadBankedDriver::SpadBankedDriver(std::string /*name*/, IMPL_CTOR) {
  phase_q_ <= phase_d_;
  UPDATE(updateDrive)
      .reads(phase_q_)
      .writes(write_val, write_bits, read_req_val, read_req_bits, read_resp_rdy);
  UPDATE(updateCheck)
      .reads(phase_q_, write_val, write_rdy, read_req_val, read_req_rdy)
      .reads(read_resp_val, read_resp_bits, read_resp_rdy, dma_resp)
      .writes(phase_d_);
}

void SpadBankedDriver::updateDrive() {
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    write_val[bank] = 0;
    write_bits[bank] = DmaReadResp{};
    read_req_val[bank] = 0;
    read_req_bits[bank] = SpadBankReadReq{};
    read_resp_rdy[bank] = 0;
  }

  switch (static_cast<std::uint8_t>(*phase_q_)) {
    case 0: // Concurrent writes to independent banks.
      for (std::size_t bank = 0; bank < 3; ++bank) {
        write_val[bank] = 1;
        write_bits[bank] = writeFor(bank, kRow0, static_cast<Elem>(10 * bank));
      }
      break;
    case 1: // Read bank 1 while writing bank 3.
      read_req_val[1] = 1;
      read_req_bits[1] = readFor(kRow0, 11);
      write_val[3] = 1;
      write_bits[3] = writeFor(3, kRow0, 30);
      break;
    case 2: // Stall bank 1 while banks 0 and 2 accept concurrent reads.
      read_req_val[0] = 1;
      read_req_bits[0] = readFor(kRow0, 20);
      read_req_val[1] = 1;
      read_req_bits[1] = readFor(kRow1, 21);
      read_req_val[2] = 1;
      read_req_bits[2] = readFor(kRow0, 22);
      break;
    case 3: // Pop bank 0 and replace its response in the same cycle.
      read_resp_rdy[0] = 1;
      read_resp_rdy[1] = 1;
      read_resp_rdy[2] = 1;
      read_req_val[0] = 1;
      read_req_bits[0] = readFor(kRow1, 30);
      break;
    case 4: // A same-bank write excludes a read on a single-ported bank.
      read_resp_rdy[0] = 1;
      write_val[3] = 1;
      write_bits[3] = writeFor(3, kRow1, 40);
      read_req_val[3] = 1;
      read_req_bits[3] = readFor(kRow1, 40);
      break;
    case 5:
      read_req_val[3] = 1;
      read_req_bits[3] = readFor(kRow1, 50);
      break;
    case 6:
      read_resp_rdy[3] = 1;
      break;
    default:
      break;
  }
}

void SpadBankedDriver::updateCheck() {
  auto phase = static_cast<std::uint8_t>(*phase_q_);
  bool advance = false;

  if (!dma_resp.empty()) {
    dma_resp.pop();
  }

  switch (phase) {
    case 0:
      advance = write_rdy[0] == 1 && write_rdy[1] == 1 && write_rdy[2] == 1;
      break;
    case 1:
      passed_ &= read_req_rdy[1] == 1 && write_rdy[3] == 1;
      advance = read_req_rdy[1] == 1 && write_rdy[3] == 1;
      break;
    case 2:
      passed_ &= read_resp_val[1] == 1;
      passed_ &= rowMatches(*read_resp_bits[1], 10);
      passed_ &= read_req_rdy[1] == 0;
      passed_ &= read_req_rdy[0] == 1 && read_req_rdy[2] == 1;
      advance = read_req_rdy[0] == 1 && read_req_rdy[2] == 1;
      break;
    case 3:
      passed_ &= read_resp_val[0] == 1 && rowMatches(*read_resp_bits[0], 0);
      passed_ &= read_resp_val[2] == 1 && rowMatches(*read_resp_bits[2], 20);
      passed_ &= read_req_rdy[0] == 1;
      advance = read_req_rdy[0] == 1;
      break;
    case 4:
      passed_ &= read_resp_val[0] == 1;
      passed_ &= read_resp_bits[0]->cmd_id == 30;
      passed_ &= write_rdy[3] == 1 && read_req_rdy[3] == 0;
      advance = write_rdy[3] == 1;
      break;
    case 5:
      passed_ &= read_req_rdy[3] == 1;
      advance = read_req_rdy[3] == 1;
      break;
    case 6:
      passed_ &= read_resp_val[3] == 1;
      passed_ &= rowMatches(*read_resp_bits[3], 40);
      if (read_resp_val[3] == 1) {
        done_ = true;
        advance = true;
      }
      break;
    default:
      break;
  }

  phase_d_ = advance ? static_cast<std::uint8_t>(phase + 1) : phase;
}

void SpadBankedDriver::reset() {
  phase_d_.reset(0);
  done_ = false;
  passed_ = true;
  for (std::size_t bank = 0; bank < kSpBanks; ++bank) {
    write_val[bank].reset(0);
    write_bits[bank].reset(DmaReadResp{});
    read_req_val[bank].reset(0);
    read_req_bits[bank].reset(SpadBankReadReq{});
    read_resp_rdy[bank].reset(0);
  }
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::Spad spad("Spad");
  SpadBankedDriver driver("Driver");

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad.write_val_bnk[bank] << driver.write_val[bank];
    spad.write_bits_bnk[bank] << driver.write_bits[bank];
    driver.write_rdy[bank] << spad.write_rdy_bnk[bank];
    spad.read_req_val_bnk[bank] << driver.read_req_val[bank];
    spad.read_req_bits_bnk[bank] << driver.read_req_bits[bank];
    driver.read_req_rdy[bank] << spad.read_req_rdy_bnk[bank];
    driver.read_resp_val[bank] << spad.read_resp_val_bnk[bank];
    driver.read_resp_bits[bank] << spad.read_resp_bits_bnk[bank];
    spad.read_resp_rdy_bnk[bank] << driver.read_resp_rdy[bank];
  }
  driver.dma_resp << spad.dma_resp;

  Clock clk;
  spad.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 16 && !driver.done(); ++cycle) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.passed();
  std::printf("[SPAD_BANKED] %s independent_bank_handshakes\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
