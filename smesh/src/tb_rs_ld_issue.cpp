// **********************************************************************
// smesh/src/tb_rs_ld_issue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
// Focused SmeshRS issue.ld to LdCtrl command-transfer test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrl.hpp"
#include "SmeshCommand.hpp"
#include "SmeshRS.hpp"

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

class DmaReqSink : public Component {
  DECLARE_COMPONENT(DmaReqSink);

 public:
  DmaReqSink(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoInput(smesh::DmaReadReq, req_in);

  void update();
  void reset();

  bool hasRequest() const { return received_; }
  const smesh::DmaReadReq& request() const { return request_; }

 private:
  bool received_ = false;
  smesh::DmaReadReq request_{};
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
  cmd.rs1 = u64(0x1000);
  cmd.rs2 = u64(smesh::packLocal(smesh::makeSpAddr(0), shape));
  alloc_out.push(cmd);
  sent_ = true;
}

void RsAllocDriver::reset() {
  sent_ = false;
}

DmaReqSink::DmaReqSink(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(req_in);
}

void DmaReqSink::update() {
  if (received_ || req_in.empty()) {
    return;
  }

  request_ = req_in.pop();
  received_ = true;
}

void DmaReqSink::reset() {
  received_ = false;
  request_ = {};
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  RsAllocDriver driver("Driver");
  smesh::SmeshRS rs("RS");
  smesh::LdCtrl ld_ctrl("LdCtrl");
  DmaReqSink dma_sink("DmaSink");

  rs.alloc_in << driver.alloc_out;
  ld_ctrl.cmd_in << rs.issue_ld;
  dma_sink.req_in << ld_ctrl.dma_req;
  ld_ctrl.completed.sendToBitBucket();
  ld_ctrl.completed.wireToZero();
  rs.setLoadIssuePortEnabled(true);

  Clock clk;
  driver.clk << clk;
  rs.clk << clk;
  ld_ctrl.clk << clk;
  dma_sink.clk << clk;
  clk.generateClock();

  Sim::init();
  Sim::reset();
  rs.setLoadIssuePortEnabled(true);
  for (int i = 0; i < 8 && !dma_sink.hasRequest(); ++i) {
    Sim::run();
  }

  const auto& issue = ld_ctrl.activeCommand();
  const auto& req = dma_sink.request();
  const bool command_ok = ld_ctrl.hasActiveCommand() &&
                          issue.rob_id == 0 &&
                          static_cast<std::uint32_t>(issue.cmd.funct) ==
                              static_cast<std::uint32_t>(smesh::SmeshFunct::Mvin);
  const bool request_ok = dma_sink.hasRequest() &&
                          static_cast<std::uint64_t>(req.vaddr) == 0x1000 &&
                          req.laddr.raw == smesh::makeSpAddr(0).raw &&
                          static_cast<std::uint16_t>(req.cols) == smesh::kDim &&
                          static_cast<std::uint16_t>(req.block_stride) == smesh::kDim &&
                          static_cast<std::uint16_t>(req.cmd_id) == 0;
  const bool ok = command_ok && request_ok;
  std::printf("[RS_LD_ISSUE] %s command_transfer_and_dma_request\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
