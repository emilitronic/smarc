// **********************************************************************
// smesh/src/tb_smesh_top_acc_full_load.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 24 2026
// Focused full-width accumulator load-return path test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Accum.hpp"
#include "ArbWriteLocal.hpp"
#include "MvinScale.hpp"
#include "SmeshPorts.hpp"
#include "WriteCtrl.hpp"

#include <array>
#include <cstdio>

class FullAccumLoadSource : public Component {
  DECLARE_COMPONENT(FullAccumLoadSource);

 public:
  FullAccumLoadSource(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::DmaReadResp, data_out);

  void update();
  void reset();

 private:
  bool sent_ = false;
};

class FullAccumLoadTieOff : public Component {
  DECLARE_COMPONENT(FullAccumLoadTieOff);

 public:
  FullAccumLoadTieOff(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, zero_bit);
  Output(bit, one_bit);
  Output(smesh::DmaReadResp, dma_read_resp);
  Output(smesh::AccumReadReq, accum_read_req);

  void update();
};

namespace {

constexpr std::array<smesh::Acc, smesh::kDim> kExpected{{0x01020304, 0x11121314, 0x21222324, 0x31323334}};

smesh::DmaReadData packAccRow(const std::array<smesh::Acc, smesh::kDim>& row) {
  smesh::DmaReadData data{};
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    const auto word = static_cast<std::uint32_t>(row[lane]);
    for (std::size_t byte = 0; byte < sizeof(smesh::Acc); ++byte) {
      data[lane * sizeof(smesh::Acc) + byte] =
          static_cast<std::uint8_t>((word >> (8 * byte)) & 0xffu);
    }
  }
  return data;
}

} // namespace

FullAccumLoadSource::FullAccumLoadSource(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(data_out);
}

void FullAccumLoadSource::update() {
  if (sent_ || data_out.full()) {
    return;
  }

  smesh::DmaReadResp resp{};
  resp.data = packAccRow(kExpected);
  resp.laddr = smesh::makeAccAddr(0);
  resp.mask = static_cast<u8>((1u << smesh::kDim) - 1u);
  resp.has_acc_bitwidth = true;
  resp.len = smesh::kDim;
  resp.bytes_read = smesh::kDim * sizeof(smesh::Acc);
  resp.cmd_id = 7;
  resp.last = true;
  data_out.push(resp);
  sent_ = true;
}

void FullAccumLoadSource::reset() {
  sent_ = false;
}

FullAccumLoadTieOff::FullAccumLoadTieOff(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(zero_bit, one_bit, dma_read_resp, accum_read_req);
}

void FullAccumLoadTieOff::update() {
  zero_bit = 0;
  one_bit = 1;
  dma_read_resp = smesh::DmaReadResp{};
  accum_read_req = smesh::AccumReadReq{};
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  FullAccumLoadSource source("Source");
  smesh::MvinScaleSplit split("MvinScaleSplit");
  smesh::MvinScaleAcc scale_acc("MvinScaleAcc");
  smesh::WriteCtrl write_ctrl("WriteCtrl");
  FullAccumLoadTieOff tie_off("TieOff");
  std::array<smesh::ArbWriteAccum*, smesh::kAccBanks> arb_accum{};
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    arb_accum[bank] = new smesh::ArbWriteAccum("ArbWriteAccum");
  }
  smesh::Accum accum("Accum");

  split.data_in << source.data_out;
  split.normal_out.sendToBitBucket();
  scale_acc.data_in << split.acc_out;
  scale_acc.data_rdy << write_ctrl.dmaread_accum_full_rdy;
  scale_acc.data_out.sendToBitBucket();

  write_ctrl.dmaread_spad_val << tie_off.zero_bit;
  write_ctrl.dmaread_spad_bits << tie_off.dma_read_resp;
  write_ctrl.dmaread_accum_val << tie_off.zero_bit;
  write_ctrl.dmaread_accum_bits << tie_off.dma_read_resp;
  write_ctrl.dmaread_accum_full_val << scale_acc.data_val;
  write_ctrl.dmaread_accum_full_bits << scale_acc.data_bits;

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    write_ctrl.arb_spad_dmaread_rdy[bank] << tie_off.zero_bit;
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    arb_accum[bank]->exwrite_val << tie_off.zero_bit;
    arb_accum[bank]->exwrite_bits << tie_off.dma_read_resp;
    arb_accum[bank]->dmaread_val << write_ctrl.arb_accum_dmaread_val[bank];
    arb_accum[bank]->dmaread_bits << write_ctrl.arb_accum_dmaread_bits[bank];
    write_ctrl.arb_accum_dmaread_rdy[bank] << arb_accum[bank]->dmaread_rdy;
    arb_accum[bank]->dmaread_full_val << write_ctrl.arb_accum_dmaread_full_val[bank];
    arb_accum[bank]->dmaread_full_bits << write_ctrl.arb_accum_dmaread_full_bits[bank];
    write_ctrl.arb_accum_dmaread_full_rdy[bank] << arb_accum[bank]->dmaread_full_rdy;
    arb_accum[bank]->zerowrite_val << tie_off.zero_bit;
    arb_accum[bank]->zerowrite_bits << tie_off.dma_read_resp;
    arb_accum[bank]->write_rdy << accum.write_rdy_bnk[bank];
    accum.write_val_bnk[bank] << arb_accum[bank]->write_val;
    accum.write_bits_bnk[bank] << arb_accum[bank]->write_bits;
  }
  accum.dma_resp.sendToBitBucket();
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum.read_req_val_bnk[bank] << tie_off.zero_bit;
    accum.read_req_bits_bnk[bank] << tie_off.accum_read_req;
    accum.read_resp_rdy_bnk[bank] << tie_off.one_bit;
  }

  Clock clk;
  source.clk << clk;
  split.clk << clk;
  scale_acc.clk << clk;
  write_ctrl.clk << clk;
  tie_off.clk << clk;
  for (auto* arb : arb_accum) {
    arb->clk << clk;
  }
  accum.clk << clk;
  clk.generateClock();

  Sim::init();
  Sim::reset();

  for (int i = 0; i < 16 && !accum.hasAcceptedWrite(); ++i) {
    Sim::run();
  }

  bool row_ok = accum.hasAcceptedWrite();
  const auto& row = accum.row(smesh::makeAccAddr(0));
  for (std::size_t lane = 0; lane < smesh::kDim; ++lane) {
    row_ok = row_ok && row[lane] == kExpected[lane];
  }

  for (auto* arb : arb_accum) {
    delete arb;
  }

  std::printf("[SMESH_TOP_ACC_FULL_LOAD] %s mvin_scale_acc_to_accum\n", row_ok ? "PASS" : "FAIL");
  return row_ok ? 0 : 1;
}
