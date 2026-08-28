// **********************************************************************
// smesh/src/tb_ex_ctrl_writeback.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026
// Focused ExCtrlWriteback structural-boundary test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlDecoder.hpp"
#include "ExCtrlWriteback.hpp"

#include <cstdio>

class ExCtrlWritebackDriver : public Component {
  DECLARE_COMPONENT(ExCtrlWritebackDriver);

 public:
  ExCtrlWritebackDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, mesh_resp_val);
  Output(smesh::MesherResp, mesh_resp_bits);
  Output(u8, current_dataflow);
  Output(u32, c_addr_stride);
  Output(u8, activation);
  Output(u32, aligned_to);
  Output(bit, ex_write_to_spad);
  Output(bit, ex_write_to_acc);
  OutputArray(bit, spad_write_rdy, smesh::kSpBanks);
  OutputArray(bit, accum_write_rdy, smesh::kAccBanks);

  void update();
  void reset();
};

class ExCtrlWritebackMonitor : public Component {
  DECLARE_COMPONENT(ExCtrlWritebackMonitor);

 public:
  ExCtrlWritebackMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  InputArray(bit, spad_write_val, smesh::kSpBanks);
  InputArray(bit, accum_write_val, smesh::kAccBanks);
  Input(bit, mesh_completed_rs_tag_fire);
  Input(bit, completed_val);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

ExCtrlWritebackDriver::ExCtrlWritebackDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(mesh_resp_val,
              mesh_resp_bits,
              current_dataflow,
              c_addr_stride,
              activation,
              aligned_to,
              ex_write_to_spad,
              ex_write_to_acc)
      .writes(
              spad_write_rdy,
              accum_write_rdy);
}

void ExCtrlWritebackDriver::update() {
  smesh::MesherResp resp{};
  resp.tag.rs_tag_valid = 1;
  resp.tag.rs_tag = 7;
  resp.last = 1;

  mesh_resp_val = 1;
  mesh_resp_bits = resp;
  current_dataflow = smesh::kExDataflowWS;
  c_addr_stride = 1;
  activation = 0;
  aligned_to = 0;
  ex_write_to_spad = 1;
  ex_write_to_acc = 1;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_write_rdy[bank] = 1;
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_write_rdy[bank] = 1;
  }
}

void ExCtrlWritebackDriver::reset() {
  mesh_resp_val.reset(0);
  mesh_resp_bits.reset(smesh::MesherResp{});
  current_dataflow.reset(smesh::kExDataflowWS);
  c_addr_stride.reset(1);
  activation.reset(0);
  aligned_to.reset(0);
  ex_write_to_spad.reset(0);
  ex_write_to_acc.reset(0);
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_write_rdy[bank].reset(0);
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_write_rdy[bank].reset(0);
  }
}

ExCtrlWritebackMonitor::ExCtrlWritebackMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(spad_write_val,
             accum_write_val,
             mesh_completed_rs_tag_fire,
             completed_val);
}

void ExCtrlWritebackMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  bool no_writes = true;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    no_writes = no_writes && spad_write_val[bank] == 0;
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    no_writes = no_writes && accum_write_val[bank] == 0;
  }

  passed_ =
      no_writes &&
      mesh_completed_rs_tag_fire == 0 &&
      completed_val == 0;
  checked_ = true;
  done_ = true;
}

void ExCtrlWritebackMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  ExCtrlWritebackDriver driver("Driver");
  smesh::ExCtrlWriteback writeback("ExCtrlWriteback");
  ExCtrlWritebackMonitor monitor("Monitor");

  writeback.mesh_resp_val << driver.mesh_resp_val;
  writeback.mesh_resp_bits << driver.mesh_resp_bits;
  writeback.current_dataflow << driver.current_dataflow;
  writeback.c_addr_stride << driver.c_addr_stride;
  writeback.activation << driver.activation;
  writeback.aligned_to << driver.aligned_to;
  writeback.ex_write_to_spad << driver.ex_write_to_spad;
  writeback.ex_write_to_acc << driver.ex_write_to_acc;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    writeback.spad_write_rdy[bank] << driver.spad_write_rdy[bank];
    monitor.spad_write_val[bank] << writeback.spad_write_val[bank];
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    writeback.accum_write_rdy[bank] << driver.accum_write_rdy[bank];
    monitor.accum_write_val[bank] << writeback.accum_write_val[bank];
  }

  monitor.mesh_completed_rs_tag_fire << writeback.mesh_completed_rs_tag_fire;
  monitor.completed_val << writeback.completed_val;

  Clock clk;
  driver.clk << clk;
  writeback.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_WRITEBACK] %s skeleton_boundary\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
