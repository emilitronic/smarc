// **********************************************************************
// smesh/tests/unit/ld_ctrl/tb_ld_ctrl2.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "LdCtrl.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

namespace {

smesh::SmeshIssue issue(smesh::SmeshFunct funct, std::uint64_t rs1,
                        std::uint64_t rs2, smesh::SmeshRsTag tag) {
  smesh::SmeshIssue result{};
  result.cmd.funct = static_cast<std::uint32_t>(funct);
  result.cmd.rs1 = rs1;
  result.cmd.rs2 = rs2;
  result.rs_tag = tag;
  return result;
}

class LoadDriver : public Component {
  DECLARE_COMPONENT(LoadDriver);

 public:
  LoadDriver(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Output(bit, cmd_val);
  Output(smesh::SmeshIssue, cmd_bits);
  Output(bit, dma_req_rdy);
  Output(bit, dma_resp_val);
  Output(smesh::DmaReadCompletion, dma_resp_bits);
  Output(bit, completed_rdy);
  void update();
  void reset();

 private:
  unsigned cycle_ = 0;
};

class LoadMonitor : public Component {
  DECLARE_COMPONENT(LoadMonitor);

 public:
  LoadMonitor(std::string name, COMPONENT_CTOR);
  Clock(clk);
  Input(bit, cmd_rdy);
  Input(bit, dma_req_val);
  Input(bit, dma_req_rdy);
  Input(smesh::DmaReadReq, dma_req_bits);
  Input(bit, completed_val);
  Input(bit, completed_rdy);
  Input(smesh::SmeshRsTag, completed_bits);
  Input(bit, busy);
  Input(u8, control_state);
  void update();
  void reset();
  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  unsigned cycle_ = 0;
  unsigned requests_ = 0;
  unsigned completions_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

LoadDriver::LoadDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).writes(cmd_val, cmd_bits, dma_req_rdy,
                        dma_resp_val, dma_resp_bits, completed_rdy);
}

void LoadDriver::update() {
  cmd_val = bit(cycle_ < 4);
  smesh::SmeshIssue command{};
  switch (cycle_) {
    case 0:
      command = issue(smesh::SmeshFunct::Config,
                      smesh::packConfig(smesh::ConfigKind::Load, 0, 4), 16, 1);
      break;
    case 1:
      command = issue(smesh::SmeshFunct::Mvin, 0x1000,
                      smesh::packLocal(smesh::makeSpAddr(4), {3, 4}), 11);
      break;
    case 2:
      command = issue(smesh::SmeshFunct::Config,
                      smesh::packConfig(smesh::ConfigKind::Load, 1, 4), 0, 2);
      break;
    case 3:
      command = issue(smesh::SmeshFunct::Mvin2, 0x2000,
                      smesh::packLocal(smesh::makeSpAddr(8), {3, 4}), 12);
      break;
    default:
      break;
  }
  cmd_bits = command;
  dma_req_rdy = bit(cycle_ != 2 && cycle_ != 3);
  completed_rdy = bit(cycle_ >= 15);
  dma_resp_val = bit(cycle_ >= 10 && cycle_ <= 13);
  smesh::DmaReadCompletion response{};
  response.cmd_id = cycle_ == 13 ? 1 : 0;
  response.bytes_read = 4;
  dma_resp_bits = response;
  ++cycle_;
}

void LoadDriver::reset() {
  cycle_ = 0;
  cmd_val.reset(0);
  cmd_bits.reset(smesh::SmeshIssue{});
  dma_req_rdy.reset(0);
  dma_resp_val.reset(0);
  dma_resp_bits.reset(smesh::DmaReadCompletion{});
  completed_rdy.reset(0);
}

LoadMonitor::LoadMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cmd_rdy, dma_req_val, dma_req_rdy, dma_req_bits,
                       completed_val, completed_rdy, completed_bits, busy)
                .reads(control_state);
}

void LoadMonitor::update() {
  if (Sim::state == Sim::SimResetting) {
    return;
  }

  if (cycle_ < 4) {
    passed_ &= cmd_rdy == 1;
  }
  if (cycle_ == 2 || cycle_ == 3) {
    passed_ &= dma_req_val == 1;
    passed_ &= dma_req_bits->vaddr == 0x1000;
    passed_ &= dma_req_bits->laddr.raw == 4;
  }
  if (dma_req_val == 1) {
    const auto req = *dma_req_bits;
    const std::array<std::uint64_t, 4> vaddrs{0x1000, 0x1010, 0x1020, 0x2000};
    const std::array<std::uint32_t, 4> laddrs{4, 5, 6, 8};
    passed_ &= requests_ < vaddrs.size();
    if (requests_ < vaddrs.size()) {
      passed_ &= static_cast<std::uint64_t>(req.vaddr) == vaddrs[requests_];
      passed_ &= req.laddr.raw == laddrs[requests_];
      passed_ &= static_cast<unsigned>(req.cols) == 4;
      passed_ &= static_cast<unsigned>(req.repeats) == (requests_ == 3 ? 2u : 0u);
      passed_ &= static_cast<unsigned>(req.cmd_id) == (requests_ == 3 ? 1u : 0u);
      passed_ &= static_cast<unsigned>(req.block_stride) == 4;
      passed_ &= static_cast<unsigned>(req.pixel_repeats) == 1;
      passed_ &= req.all_zeros == 0 && req.has_acc_bitwidth == 0;
    }
    std::printf("[c%02u] req id=%u vaddr=%llx laddr=%u repeats=%u ready=%u\n",
                cycle_, static_cast<unsigned>(req.cmd_id),
                static_cast<unsigned long long>(req.vaddr), req.laddr.raw,
                static_cast<unsigned>(req.repeats),
                static_cast<unsigned>(dma_req_rdy == 1));
    if (dma_req_rdy == 1) {
      ++requests_;
    }
  }
  if (cycle_ == 13 || cycle_ == 14) {
    passed_ &= completed_val == 1 && *completed_bits == 11;
  }
  if (completed_val == 1) {
    const smesh::SmeshRsTag expected[] = {11, 12};
    passed_ &= completions_ < 2;
    if (completions_ < 2) {
      passed_ &= *completed_bits == expected[completions_];
    }
    std::printf("[c%02u] complete tag=%u ready=%u\n",
                cycle_, static_cast<unsigned>(completed_bits),
                static_cast<unsigned>(completed_rdy == 1));
    if (completed_rdy == 1) {
      ++completions_;
    }
  }
  if (cycle_ == 20) {
    passed_ &= requests_ == 4 && completions_ == 2 && busy == 0;
    done_ = true;
  }
  ++cycle_;
}

void LoadMonitor::reset() {
  cycle_ = 0;
  requests_ = 0;
  completions_ = 0;
  done_ = false;
  passed_ = true;
}

} // namespace

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::LdCtrl controller("LoadController");
  LoadDriver driver("Driver");
  LoadMonitor monitor("Monitor");
  controller.cmd_val << driver.cmd_val;
  controller.cmd_bits << driver.cmd_bits;
  controller.dma_req_rdy << driver.dma_req_rdy;
  controller.dma_resp_val << driver.dma_resp_val;
  controller.dma_resp_bits << driver.dma_resp_bits;
  controller.completed_rdy << driver.completed_rdy;
  monitor.cmd_rdy << controller.cmd_rdy;
  monitor.dma_req_val << controller.dma_req_val;
  monitor.dma_req_rdy << driver.dma_req_rdy;
  monitor.dma_req_bits << controller.dma_req_bits;
  monitor.completed_val << controller.completed_val;
  monitor.completed_rdy << driver.completed_rdy;
  monitor.completed_bits << controller.completed_bits;
  monitor.busy << controller.busy;
  monitor.control_state << controller.control_state;

  Clock clk;
  controller.clk << clk;
  driver.clk << clk;
  monitor.clk << clk;
  clk.generateClock();
  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 24 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[LD_CTRL2] %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
