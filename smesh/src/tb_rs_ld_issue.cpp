// **********************************************************************
// smesh/src/tb_rs_ld_issue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
// Focused SmeshRS issue.ld to LdCtrl command-transfer test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ArbWriteLocal.hpp"
#include "DmaReader.hpp"
#include "LdCtrl.hpp"
#include "MvinLocalRouter.hpp"
#include "MvinPixelRepeater.hpp"
#include "MvinScale.hpp"
#include "SmeshCommand.hpp"
#include "SmeshRS.hpp"
#include "Spad.hpp"
#include "WriteCtrl.hpp"
#include "smem/Dram.hpp"
#include "smem/MemCtrl.hpp"

#include <array>
#include <cstdio>

constexpr std::uint64_t kDramBase = 0x80001000;
constexpr std::uint32_t kDramRowStride = 7;
constexpr std::uint32_t kLoadBlockStride = 6;

class RsAllocDriver : public Component {
  DECLARE_COMPONENT(RsAllocDriver);

 public:
  RsAllocDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshCmd, alloc_out);

  void update();
  void reset();

 private:
  std::uint32_t next_command_ = 0;
};

class ZeroSpadReadDriver : public Component {
  DECLARE_COMPONENT(ZeroSpadReadDriver);

 public:
  ZeroSpadReadDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, zero_bit);
  Output(bit, one_bit);
  Output(smesh::DmaReadResp, dma_read_resp);
  Output(smesh::SpadReadReq, read_req);

  void update();
};

RsAllocDriver::RsAllocDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(alloc_out);
}

void RsAllocDriver::update() {
  if (next_command_ >= 2 || alloc_out.full()) {
    return;
  }

  constexpr smesh::MatrixShape shape{smesh::kDim, smesh::kDim};
  smesh::SmeshCmd cmd{};
  if (next_command_ == 0) {
    cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Config));
    cmd.rs1 = u64(smesh::packConfig(smesh::ConfigKind::Load, 0, kLoadBlockStride));
    cmd.rs2 = u64(kDramRowStride);
  } else {
    cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin));
    cmd.rs1 = u64(kDramBase);
    cmd.rs2 = u64(smesh::packLocal(smesh::makeSpAddr(0), shape));
  }
  alloc_out.push(cmd);
  ++next_command_;
}

void RsAllocDriver::reset() {
  next_command_ = 0;
}

ZeroSpadReadDriver::ZeroSpadReadDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(zero_bit, one_bit, dma_read_resp, read_req);
}

void ZeroSpadReadDriver::update() {
  zero_bit = 0;
  one_bit = 1;
  dma_read_resp = smesh::DmaReadResp{};
  read_req = smesh::SpadReadReq{};
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  RsAllocDriver driver("Driver");
  smesh::SmeshRS rs("RS");
  smesh::LdCtrl ld_ctrl("LdCtrl");
  smesh::DmaReader dma_reader("DmaReader");
  smesh::MvinScale mvin_scale("MvinScale");
  smesh::MvinPixelRepeater pixel_repeater("MvinPixelRepeater");
  smesh::MvinLocalRouter local_router("MvinLocalRouter");
  smesh::WriteCtrl write_ctrl("WriteCtrl");
  std::array<smesh::ArbWriteSpad*, smesh::kSpBanks> arb_spad{};
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    arb_spad[bank] = new smesh::ArbWriteSpad("ArbWriteSpad");
  }
  smesh::Spad spad("Spad");
  ZeroSpadReadDriver zero_spad_read("ZeroSpadRead");
  smem::MemCtrl mem("MemCtrl");
  smem::Dram dram("Dram", 0);

  rs.alloc_in << driver.alloc_out;
  ld_ctrl.cmd_in << rs.issue_ld;
  rs.issue_ex.sendToBitBucket();
  rs.issue_st.sendToBitBucket();
  rs.completed << ld_ctrl.completed;
  dma_reader.req_in << ld_ctrl.dma_req;
  mem.in_core_req << dma_reader.mem_req;
  dma_reader.mem_resp << mem.out_core_resp;
  mvin_scale.data_in << dma_reader.resp_out;
  pixel_repeater.data_in << mvin_scale.data_out;
  local_router.data_in << pixel_repeater.data_out;
  local_router.dmaread_spad_rdy << write_ctrl.dmaread_spad_rdy;
  local_router.dmaread_accum_rdy << zero_spad_read.zero_bit;
  local_router.dmaread_spad.sendToBitBucket();
  local_router.dmaread_accum.sendToBitBucket();
  write_ctrl.dmaread_spad_val << local_router.dmaread_spad_val;
  write_ctrl.dmaread_spad_bits << local_router.dmaread_spad_bits;
  write_ctrl.dmaread_accum_val << zero_spad_read.zero_bit;
  write_ctrl.dmaread_accum_bits << zero_spad_read.dma_read_resp;
  write_ctrl.dmaread_accum_full_val << zero_spad_read.zero_bit;
  write_ctrl.dmaread_accum_full_bits << zero_spad_read.dma_read_resp;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    arb_spad[bank]->exwrite_val << zero_spad_read.zero_bit;
    arb_spad[bank]->exwrite_bits << zero_spad_read.dma_read_resp;
    arb_spad[bank]->dmaread_val << write_ctrl.arb_spad_dmaread_val[bank];
    arb_spad[bank]->dmaread_bits << write_ctrl.arb_spad_dmaread_bits[bank];
    write_ctrl.arb_spad_dmaread_rdy[bank] << arb_spad[bank]->dmaread_rdy;
    arb_spad[bank]->zerowrite_val << zero_spad_read.zero_bit;
    arb_spad[bank]->zerowrite_bits << zero_spad_read.dma_read_resp;
    arb_spad[bank]->write_rdy << spad.write_rdy_bnk[bank];
    spad.write_val_bnk[bank] << arb_spad[bank]->write_val;
    spad.write_bits_bnk[bank] << arb_spad[bank]->write_bits;
    spad.read_req_val_bnk[bank] << zero_spad_read.zero_bit;
    spad.read_req_bits_bnk[bank] << zero_spad_read.read_req;
    spad.read_resp_rdy_bnk[bank] << zero_spad_read.one_bit;
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    write_ctrl.arb_accum_dmaread_rdy[bank] << zero_spad_read.zero_bit;
    write_ctrl.arb_accum_dmaread_full_rdy[bank] << zero_spad_read.zero_bit;
  }
  ld_ctrl.dma_resp << spad.dma_resp;
  mem.in_core_req.setDelay(1);
  dram.s_req << mem.s_req;
  mem.s_resp << dram.s_resp;
  rs.setLoadIssuePortEnabled(true);

  Clock clk;
  driver.clk << clk;
  rs.clk << clk;
  ld_ctrl.clk << clk;
  dma_reader.clk << clk;
  mvin_scale.clk << clk;
  pixel_repeater.clk << clk;
  local_router.clk << clk;
  write_ctrl.clk << clk;
  for (auto* arb : arb_spad) {
    arb->clk << clk;
  }
  spad.clk << clk;
  zero_spad_read.clk << clk;
  mem.clk << clk;
  dram.clk << clk;
  clk.generateClock();

  Sim::init();
  Sim::reset();
  const std::array<std::uint8_t, smesh::kDim * smesh::kDim> rows{{
      0x11, 0x12, 0x13, 0x14,
      0x21, 0x22, 0x23, 0x24,
      0x31, 0x32, 0x33, 0x34,
      0x41, 0x42, 0x43, 0x44,
  }};
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    dram.write(kDramBase + r * kDramRowStride,
               rows.data() + r * smesh::kDim,
               smesh::kDim);
  }
  rs.setLoadIssuePortEnabled(true);
  for (int i = 0; i < 96 && !(ld_ctrl.hasDmaResponse() && rs.empty()); ++i) {
    Sim::run();
  }

  const auto& issue = ld_ctrl.activeCommand();
  const auto& req = dma_reader.activeRequest();
  const bool command_ok = !ld_ctrl.hasActiveCommand() &&
                          issue.rs_tag == 1 &&
                          static_cast<std::uint32_t>(issue.cmd.funct) ==
                              static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin);
  const bool request_ok = static_cast<std::uint64_t>(req.vaddr) ==
                              kDramBase + (smesh::kDim - 1) * kDramRowStride &&
                          req.laddr.raw == smesh::makeSpAddr(3).raw &&
                          static_cast<std::uint16_t>(req.cols) == smesh::kDim &&
                          static_cast<std::uint16_t>(req.block_stride) == kLoadBlockStride &&
                          static_cast<std::uint16_t>(req.cmd_id) == 1;
  bool spad_ok = spad.hasAcceptedWrite();
  for (std::size_t r = 0; r < smesh::kDim; ++r) {
    const auto& spad_row = spad.row(smesh::makeSpAddr(static_cast<std::uint32_t>(r)));
    for (std::size_t c = 0; c < smesh::kDim; ++c) {
      spad_ok = spad_ok &&
                spad_row[c] == static_cast<smesh::Elem>(rows[r * smesh::kDim + c]);
    }
  }
  const bool completion_ok = ld_ctrl.hasDmaResponse() &&
                             ld_ctrl.expectedBytes() == smesh::kDim * smesh::kDim &&
                             ld_ctrl.returnedBytes() == smesh::kDim * smesh::kDim &&
                             ld_ctrl.responseRsTag() == 1 &&
                             rs.empty();
  const bool ok = command_ok && request_ok && spad_ok && completion_ok;
  for (auto* arb : arb_spad) {
    delete arb;
  }
  std::printf("[RS_LD_ISSUE] %s dma_spad_write_completion\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
