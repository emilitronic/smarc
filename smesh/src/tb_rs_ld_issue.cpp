// **********************************************************************
// smesh/src/tb_rs_ld_issue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
// Focused SmeshRS issue.ld to LdCtrl command-transfer test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "DmaReader.hpp"
#include "LdCtrl.hpp"
#include "MvinPixelRepeater.hpp"
#include "MvinScale.hpp"
#include "SmeshCommand.hpp"
#include "SmeshRS.hpp"
#include "smem/Dram.hpp"
#include "smem/MemCtrl.hpp"

#include <array>
#include <cstdio>

class RsAllocDriver : public Component {
  DECLARE_COMPONENT(RsAllocDriver);

 public:
  RsAllocDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshCmd, alloc_out);

  void update();
  void reset();

 private:
  bool sent_ = false;
};

class DmaRespSink : public Component {
  DECLARE_COMPONENT(DmaRespSink);

 public:
  DmaRespSink(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoInput(smesh::DmaReadResp, resp_in);

  void update();
  void reset();

  bool hasResponse() const { return received_; }
  const smesh::DmaReadResp& response() const { return response_; }

 private:
  bool received_ = false;
  smesh::DmaReadResp response_{};
};

RsAllocDriver::RsAllocDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(alloc_out);
}

void RsAllocDriver::update() {
  if (sent_ || alloc_out.full()) {
    return;
  }

  constexpr smesh::MatrixShape shape{smesh::kDim, smesh::kDim};
  smesh::SmeshCmd cmd{};
  cmd.funct = u32(static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin));
  cmd.rs1 = u64(0x80001000);
  cmd.rs2 = u64(smesh::packLocal(smesh::makeSpAddr(0), shape));
  alloc_out.push(cmd);
  sent_ = true;
}

void RsAllocDriver::reset() {
  sent_ = false;
}

DmaRespSink::DmaRespSink(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(resp_in);
}

void DmaRespSink::update() {
  if (received_ || resp_in.empty()) {
    return;
  }

  response_ = resp_in.pop();
  received_ = true;
}

void DmaRespSink::reset() {
  received_ = false;
  response_ = {};
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
  DmaRespSink dma_sink("DmaSink");
  smem::MemCtrl mem("MemCtrl");
  smem::Dram dram("Dram", 0);

  rs.alloc_in << driver.alloc_out;
  ld_ctrl.cmd_in << rs.issue_ld;
  dma_reader.req_in << ld_ctrl.dma_req;
  mem.in_core_req << dma_reader.mem_req;
  dma_reader.mem_resp << mem.out_core_resp;
  mvin_scale.data_in << dma_reader.resp_out;
  pixel_repeater.data_in << mvin_scale.data_out;
  dma_sink.resp_in << pixel_repeater.data_out;
  mem.in_core_req.setDelay(1);
  dram.s_req << mem.s_req;
  mem.s_resp << dram.s_resp;
  ld_ctrl.completed.sendToBitBucket();
  ld_ctrl.completed.wireToZero();
  rs.setLoadIssuePortEnabled(true);

  Clock clk;
  driver.clk << clk;
  rs.clk << clk;
  ld_ctrl.clk << clk;
  dma_reader.clk << clk;
  mvin_scale.clk << clk;
  pixel_repeater.clk << clk;
  dma_sink.clk << clk;
  mem.clk << clk;
  dram.clk << clk;
  clk.generateClock();

  Sim::init();
  Sim::reset();
  const std::array<std::uint8_t, smesh::kDim> row{{0x11, 0x22, 0x33, 0x44}};
  dram.write(0x80001000, row.data(), row.size());
  rs.setLoadIssuePortEnabled(true);
  for (int i = 0; i < 16 && !dma_sink.hasResponse(); ++i) {
    Sim::run();
  }

  const auto& issue = ld_ctrl.activeCommand();
  const auto& req = dma_reader.activeRequest();
  const auto& resp = dma_sink.response();
  const bool command_ok = ld_ctrl.hasActiveCommand() &&
                          issue.rob_id == 0 &&
                          static_cast<std::uint32_t>(issue.cmd.funct) ==
                              static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin);
  const bool request_ok = static_cast<std::uint64_t>(req.vaddr) == 0x80001000 &&
                          req.laddr.raw == smesh::makeSpAddr(0).raw &&
                          static_cast<std::uint16_t>(req.cols) == smesh::kDim &&
                          static_cast<std::uint16_t>(req.block_stride) == smesh::kDim &&
                          static_cast<std::uint16_t>(req.cmd_id) == 0;
  const bool response_ok = dma_sink.hasResponse() &&
                           static_cast<std::uint64_t>(resp.data) == 0x44332211 &&
                           resp.laddr.raw == smesh::makeSpAddr(0).raw &&
                           static_cast<std::uint8_t>(resp.mask) == 0x0f &&
                           static_cast<std::uint16_t>(resp.bytes_read) == smesh::kDim &&
                           static_cast<std::uint8_t>(resp.pixel_repeats) == 1 &&
                           static_cast<std::uint16_t>(resp.cmd_id) == 0 &&
                           static_cast<bool>(resp.last);
  const bool ok = command_ok && request_ok && response_ok;
  std::printf("[RS_LD_ISSUE] %s dma_memory_read\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
