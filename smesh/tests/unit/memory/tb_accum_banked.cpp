// **********************************************************************
// smesh/tests/unit/memory/tb_accum_banked.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 20 2026
// Focused checks (all 6 requested banking cases)for independent Accum 
// banks and accumulator arbiters.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Accum.hpp"
#include "ArbReadLocal.hpp"
#include "ArbWriteLocal.hpp"

#include <cstdio>

namespace {

using namespace smesh;

constexpr std::uint32_t kRow0 = 3;
constexpr std::uint32_t kRow1 = 2;

DmaReadResp writeFor(std::size_t bank, std::uint32_t row, Acc base,
                     std::uint16_t cmd_id = 0) {
  DmaReadResp write{};
  write.laddr = makeAccAddr(static_cast<std::uint32_t>(bank * kAccBankRows) + row);
  write.mask = static_cast<std::uint8_t>((1u << kDim) - 1u);
  write.has_acc_bitwidth = true;
  write.cmd_id = cmd_id;
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    const auto word = static_cast<std::uint32_t>(base + static_cast<Acc>(lane));
    for (std::size_t byte = 0; byte < sizeof(Acc); ++byte) {
      write.data[lane * sizeof(Acc) + byte] =
          static_cast<std::uint8_t>((word >> (8 * byte)) & 0xffu);
    }
  }
  return write;
}

AccumBankReadReq readFor(std::uint32_t row, std::uint16_t cmd_id) {
  AccumBankReadReq read{};
  read.addr = row;
  read.len = kDim;
  read.cmd_id = cmd_id;
  return read;
}

bool rowMatches(const AccumReadResp& response, Acc base) {
  for (std::size_t lane = 0; lane < kDim; ++lane) {
    if (response.data[lane] != base + static_cast<Acc>(lane)) {
      return false;
    }
  }
  return true;
}

class AccumBankedDriver : public Component {
  DECLARE_COMPONENT(AccumBankedDriver);

 public:
  AccumBankedDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  OutputArray(bit, write_val, kAccBanks);
  OutputArray(DmaReadResp, write_bits, kAccBanks);
  InputArray(bit, write_rdy, kAccBanks);
  OutputArray(bit, read_req_val, kAccBanks);
  OutputArray(AccumBankReadReq, read_req_bits, kAccBanks);
  InputArray(bit, read_req_rdy, kAccBanks);
  InputArray(bit, read_resp_val, kAccBanks);
  InputArray(AccumReadResp, read_resp_bits, kAccBanks);
  OutputArray(bit, read_resp_rdy, kAccBanks);
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

AccumBankedDriver::AccumBankedDriver(std::string /*name*/, IMPL_CTOR) {
  phase_q_ <= phase_d_;
  UPDATE(updateDrive)
      .reads(phase_q_)
      .writes(write_val, write_bits, read_req_val, read_req_bits, read_resp_rdy);
  UPDATE(updateCheck)
      .reads(phase_q_, write_val, write_rdy, read_req_val, read_req_rdy)
      .reads(read_resp_val, read_resp_bits, read_resp_rdy, dma_resp)
      .writes(phase_d_);
}

void AccumBankedDriver::updateDrive() {
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    write_val[bank] = 0;
    write_bits[bank] = DmaReadResp{};
    read_req_val[bank] = 0;
    read_req_bits[bank] = AccumBankReadReq{};
    read_resp_rdy[bank] = 0;
  }

  switch (static_cast<std::uint8_t>(*phase_q_)) {
    case 0: // Concurrent writes to both accumulator banks.
      for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
        write_val[bank] = 1;
        write_bits[bank] = writeFor(bank, kRow0, static_cast<Acc>(10 * bank));
      }
      break;
    case 1: // Read bank 1 while writing bank 0.
      read_req_val[1] = 1;
      read_req_bits[1] = readFor(kRow0, 11);
      write_val[0] = 1;
      write_bits[0] = writeFor(0, kRow1, 20);
      break;
    case 2: // Stall bank 1 while bank 0 accepts a read.
      read_req_val[0] = 1;
      read_req_bits[0] = readFor(kRow0, 20);
      read_req_val[1] = 1;
      read_req_bits[1] = readFor(kRow1, 21);
      break;
    case 3: // Pop and replace both banks' responses in the same cycle.
      for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
        read_resp_rdy[bank] = 1;
        read_req_val[bank] = 1;
      }
      read_req_bits[0] = readFor(kRow1, 30);
      read_req_bits[1] = readFor(kRow0, 31);
      break;
    case 4: // A same-bank write excludes a read on a single-ported bank.
      read_resp_rdy[0] = 1;
      read_resp_rdy[1] = 1;
      write_val[1] = 1;
      write_bits[1] = writeFor(1, kRow1, 40);
      read_req_val[1] = 1;
      read_req_bits[1] = readFor(kRow1, 40);
      break;
    case 5:
      read_req_val[1] = 1;
      read_req_bits[1] = readFor(kRow1, 50);
      break;
    case 6:
      read_resp_rdy[1] = 1;
      break;
    default:
      break;
  }
}

void AccumBankedDriver::updateCheck() {
  auto phase = static_cast<std::uint8_t>(*phase_q_);
  bool advance = false;

  if (!dma_resp.empty()) {
    dma_resp.pop();
  }

  switch (phase) {
    case 0:
      passed_ &= write_rdy[0] == 1 && write_rdy[1] == 1;
      advance = write_rdy[0] == 1 && write_rdy[1] == 1;
      break;
    case 1:
      passed_ &= read_req_rdy[1] == 1 && write_rdy[0] == 1;
      advance = read_req_rdy[1] == 1 && write_rdy[0] == 1;
      break;
    case 2:
      passed_ &= read_resp_val[1] == 1;
      passed_ &= rowMatches(*read_resp_bits[1], 10);
      passed_ &= read_req_rdy[1] == 0;
      passed_ &= read_req_rdy[0] == 1;
      advance = read_req_rdy[0] == 1;
      break;
    case 3:
      passed_ &= read_resp_val[0] == 1 && rowMatches(*read_resp_bits[0], 0);
      passed_ &= read_resp_val[1] == 1 && rowMatches(*read_resp_bits[1], 10);
      passed_ &= read_req_rdy[0] == 1 && read_req_rdy[1] == 1;
      advance = read_req_rdy[0] == 1 && read_req_rdy[1] == 1;
      break;
    case 4:
      passed_ &= read_resp_val[0] == 1 && rowMatches(*read_resp_bits[0], 20);
      passed_ &= read_resp_bits[0]->cmd_id == 30;
      passed_ &= read_resp_val[1] == 1 && rowMatches(*read_resp_bits[1], 10);
      passed_ &= read_resp_bits[1]->cmd_id == 31;
      passed_ &= write_rdy[1] == 1 && read_req_rdy[1] == 0;
      advance = write_rdy[1] == 1;
      break;
    case 5:
      passed_ &= read_req_rdy[1] == 1;
      advance = read_req_rdy[1] == 1;
      break;
    case 6:
      passed_ &= read_resp_val[1] == 1;
      passed_ &= rowMatches(*read_resp_bits[1], 40);
      if (read_resp_val[1] == 1) {
        done_ = true;
        advance = true;
      }
      break;
    default:
      break;
  }

  phase_d_ = advance ? static_cast<std::uint8_t>(phase + 1) : phase;
}

void AccumBankedDriver::reset() {
  phase_d_.reset(0);
  done_ = false;
  passed_ = true;
  for (std::size_t bank = 0; bank < kAccBanks; ++bank) {
    write_val[bank].reset(0);
    write_bits[bank].reset(DmaReadResp{});
    read_req_val[bank].reset(0);
    read_req_bits[bank].reset(AccumBankReadReq{});
    read_resp_rdy[bank].reset(0);
  }
}

class AccumArbDriver : public Component {
  DECLARE_COMPONENT(AccumArbDriver);

 public:
  AccumArbDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, one);
  Output(bit, exread_val);
  Output(AccumBankReadReq, exread_bits);
  Input(bit, exread_rdy);
  Output(bit, dmawrite_val);
  Output(AccumBankReadReq, dmawrite_bits);
  Input(bit, dmawrite_rdy);
  Input(bit, read_req_val);
  Input(AccumBankReadReq, read_req_bits);
  Output(bit, exwrite_val);
  Output(DmaReadResp, exwrite_bits);
  Input(bit, exwrite_rdy);
  Output(bit, dmaread_full_val);
  Output(DmaReadResp, dmaread_full_bits);
  Input(bit, dmaread_full_rdy);
  Output(bit, dmaread_val);
  Output(DmaReadResp, dmaread_bits);
  Input(bit, dmaread_rdy);
  Output(bit, zerowrite_val);
  Output(DmaReadResp, zerowrite_bits);
  Input(bit, zerowrite_rdy);
  Input(bit, write_val);
  Input(DmaReadResp, write_bits);

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

AccumArbDriver::AccumArbDriver(std::string /*name*/, IMPL_CTOR) {
  phase_q_ <= phase_d_;
  UPDATE(updateDrive)
      .reads(phase_q_)
      .writes(one, exread_val, exread_bits, dmawrite_val, dmawrite_bits)
      .writes(exwrite_val, exwrite_bits, dmaread_full_val, dmaread_full_bits)
      .writes(dmaread_val, dmaread_bits, zerowrite_val, zerowrite_bits);
  UPDATE(updateCheck)
      .reads(phase_q_, exread_rdy, dmawrite_rdy, read_req_val, read_req_bits)
      .reads(exwrite_rdy, dmaread_full_rdy, dmaread_rdy, zerowrite_rdy)
      .reads(write_val, write_bits)
      .writes(phase_d_);
}

void AccumArbDriver::updateDrive() {
  const auto phase = static_cast<std::uint8_t>(*phase_q_);
  one = 1;
  exread_val = bit(phase == 0);
  exread_bits = readFor(1, 101);
  dmawrite_val = bit(phase <= 1);
  dmawrite_bits = readFor(2, 102);
  exwrite_val = bit(phase == 0);
  exwrite_bits = writeFor(0, 0, 100, 201);
  dmaread_full_val = bit(phase <= 1);
  dmaread_full_bits = writeFor(0, 0, 200, 202);
  dmaread_val = bit(phase <= 2);
  dmaread_bits = writeFor(0, 0, 300, 203);
  zerowrite_val = bit(phase <= 3);
  zerowrite_bits = writeFor(0, 0, 0, 204);
}

void AccumArbDriver::updateCheck() {
  const auto phase = static_cast<std::uint8_t>(*phase_q_);
  bool phase_ok = true;

  if (phase == 0) {
    phase_ok &= exread_rdy == 1 && dmawrite_rdy == 0;
    phase_ok &= read_req_val == 1 && read_req_bits->cmd_id == 101;
  } else if (phase == 1) {
    phase_ok &= exread_rdy == 0 && dmawrite_rdy == 1;
    phase_ok &= read_req_val == 1 && read_req_bits->cmd_id == 102;
  }

  if (phase <= 3) {
    phase_ok &= write_val == 1;
    phase_ok &= write_bits->cmd_id == static_cast<std::uint16_t>(201 + phase);
    phase_ok &= exwrite_rdy == 1;
    phase_ok &= dmaread_full_rdy == bit(phase >= 1);
    phase_ok &= dmaread_rdy == bit(phase >= 2);
    phase_ok &= zerowrite_rdy == bit(phase >= 3);
  }

  passed_ &= phase_ok;
  if (phase == 3) {
    done_ = true;
  }
  phase_d_ = static_cast<std::uint8_t>(phase + 1);
}

void AccumArbDriver::reset() {
  phase_d_.reset(0);
  done_ = false;
  passed_ = true;
  one.reset(1);
  exread_val.reset(0);
  exread_bits.reset(AccumBankReadReq{});
  dmawrite_val.reset(0);
  dmawrite_bits.reset(AccumBankReadReq{});
  exwrite_val.reset(0);
  exwrite_bits.reset(DmaReadResp{});
  dmaread_full_val.reset(0);
  dmaread_full_bits.reset(DmaReadResp{});
  dmaread_val.reset(0);
  dmaread_bits.reset(DmaReadResp{});
  zerowrite_val.reset(0);
  zerowrite_bits.reset(DmaReadResp{});
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::Accum accum("Accum");
  AccumBankedDriver driver("Driver");
  smesh::ArbReadAccum read_arb("ArbReadAccum");
  smesh::ArbWriteAccum write_arb("ArbWriteAccum");
  AccumArbDriver arb_driver("ArbDriver");

  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum.write_val_bnk[bank] << driver.write_val[bank];
    accum.write_bits_bnk[bank] << driver.write_bits[bank];
    driver.write_rdy[bank] << accum.write_rdy_bnk[bank];
    accum.read_req_val_bnk[bank] << driver.read_req_val[bank];
    accum.read_req_bits_bnk[bank] << driver.read_req_bits[bank];
    driver.read_req_rdy[bank] << accum.read_req_rdy_bnk[bank];
    driver.read_resp_val[bank] << accum.read_resp_val_bnk[bank];
    driver.read_resp_bits[bank] << accum.read_resp_bits_bnk[bank];
    accum.read_resp_rdy_bnk[bank] << driver.read_resp_rdy[bank];
  }
  driver.dma_resp << accum.dma_resp;

  read_arb.exread_val << arb_driver.exread_val;
  read_arb.exread_bits << arb_driver.exread_bits;
  arb_driver.exread_rdy << read_arb.exread_rdy;
  read_arb.dmawrite_val << arb_driver.dmawrite_val;
  read_arb.dmawrite_bits << arb_driver.dmawrite_bits;
  arb_driver.dmawrite_rdy << read_arb.dmawrite_rdy;
  read_arb.read_req_rdy << arb_driver.one;
  arb_driver.read_req_val << read_arb.read_req_val;
  arb_driver.read_req_bits << read_arb.read_req_bits;

  write_arb.exwrite_val << arb_driver.exwrite_val;
  write_arb.exwrite_bits << arb_driver.exwrite_bits;
  arb_driver.exwrite_rdy << write_arb.exwrite_rdy;
  write_arb.dmaread_full_val << arb_driver.dmaread_full_val;
  write_arb.dmaread_full_bits << arb_driver.dmaread_full_bits;
  arb_driver.dmaread_full_rdy << write_arb.dmaread_full_rdy;
  write_arb.dmaread_val << arb_driver.dmaread_val;
  write_arb.dmaread_bits << arb_driver.dmaread_bits;
  arb_driver.dmaread_rdy << write_arb.dmaread_rdy;
  write_arb.zerowrite_val << arb_driver.zerowrite_val;
  write_arb.zerowrite_bits << arb_driver.zerowrite_bits;
  arb_driver.zerowrite_rdy << write_arb.zerowrite_rdy;
  write_arb.write_rdy << arb_driver.one;
  arb_driver.write_val << write_arb.write_val;
  arb_driver.write_bits << write_arb.write_bits;

  Clock clk;
  accum.clk << clk;
  driver.clk << clk;
  read_arb.clk << clk;
  write_arb.clk << clk;
  arb_driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int cycle = 0; cycle < 16 && (!driver.done() || !arb_driver.done()); ++cycle) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.passed() &&
                  arb_driver.done() && arb_driver.passed();
  std::printf("[ACCUM_BANKED] %s independent_bank_handshakes\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
