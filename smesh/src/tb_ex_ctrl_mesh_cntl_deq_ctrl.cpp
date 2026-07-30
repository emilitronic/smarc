// **********************************************************************
// smesh/src/tb_ex_ctrl_mesh_cntl_deq_ctrl.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 30 2026
// Focused ExCtrlMeshCntlDeqCtrl dequeue-ready test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlMeshCntlDeqCtrl.hpp"

#include <cstdio>

class MeshCntlDeqCtrlDriver : public Component {
  DECLARE_COMPONENT(MeshCntlDeqCtrlDriver);

 public:
  MeshCntlDeqCtrlDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(smesh::ExCtrlMeshCntl, cntl_bits);
  Output(bit, mesh_a_fire);
  Output(bit, mesh_b_fire);
  Output(bit, mesh_d_fire);
  Output(bit, mesh_a_rdy);
  Output(bit, mesh_b_rdy);
  Output(bit, mesh_d_rdy);
  Output(bit, mesh_req_rdy);

  void update();
  void reset();
};

class MeshCntlDeqCtrlMonitor : public Component {
  DECLARE_COMPONENT(MeshCntlDeqCtrlMonitor);

 public:
  MeshCntlDeqCtrlMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, mesh_cntl_deq_rdy);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

MeshCntlDeqCtrlDriver::MeshCntlDeqCtrlDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(cntl_bits,
              mesh_a_fire,
              mesh_b_fire,
              mesh_d_fire,
              mesh_a_rdy,
              mesh_b_rdy,
              mesh_d_rdy,
              mesh_req_rdy);
}

void MeshCntlDeqCtrlDriver::update() {
  smesh::ExCtrlMeshCntl cntl{};
  cntl.a_fire = 1;
  cntl.b_fire = 1;
  cntl.d_fire = 1;
  cntl.first = 1;

  cntl_bits = cntl;
  mesh_a_fire = 1;
  mesh_b_fire = 0;
  mesh_d_fire = 0;
  mesh_a_rdy = 1;
  mesh_b_rdy = 0;
  mesh_d_rdy = 0;
  mesh_req_rdy = 1;
}

void MeshCntlDeqCtrlDriver::reset() {
  cntl_bits.reset(smesh::ExCtrlMeshCntl{});
  mesh_a_fire.reset(0);
  mesh_b_fire.reset(0);
  mesh_d_fire.reset(0);
  mesh_a_rdy.reset(0);
  mesh_b_rdy.reset(0);
  mesh_d_rdy.reset(0);
  mesh_req_rdy.reset(0);
}

MeshCntlDeqCtrlMonitor::MeshCntlDeqCtrlMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(mesh_cntl_deq_rdy);
}

void MeshCntlDeqCtrlMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  passed_ = mesh_cntl_deq_rdy != 0;
  checked_ = true;
  done_ = true;
}

void MeshCntlDeqCtrlMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  MeshCntlDeqCtrlDriver driver("Driver");
  smesh::ExCtrlMeshCntlDeqCtrl deq_ctrl("DeqCtrl");
  MeshCntlDeqCtrlMonitor monitor("Monitor");

  deq_ctrl.cntl_bits << driver.cntl_bits;
  deq_ctrl.mesh_a_fire << driver.mesh_a_fire;
  deq_ctrl.mesh_b_fire << driver.mesh_b_fire;
  deq_ctrl.mesh_d_fire << driver.mesh_d_fire;
  deq_ctrl.mesh_a_rdy << driver.mesh_a_rdy;
  deq_ctrl.mesh_b_rdy << driver.mesh_b_rdy;
  deq_ctrl.mesh_d_rdy << driver.mesh_d_rdy;
  deq_ctrl.mesh_req_rdy << driver.mesh_req_rdy;

  monitor.mesh_cntl_deq_rdy << deq_ctrl.mesh_cntl_deq_rdy;

  Clock clk;
  driver.clk << clk;
  deq_ctrl.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_MESH_CNTL_DEQ_CTRL] %s deq_ready\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
