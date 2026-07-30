// **********************************************************************
// smesh/src/tb_mesher.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
// Focused Mesher structural-boundary test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "Mesher.hpp"

#include <cstdio>

class MesherDriver : public Component {
  DECLARE_COMPONENT(MesherDriver);

 public:
  MesherDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, req_val);
  Output(smesh::ExCtrlMeshReq, req_bits);
  Output(bit, a_val);
  Output(smesh::ExCtrlMeshInput, a_bits);
  Output(bit, b_val);
  Output(smesh::ExCtrlMeshInput, b_bits);
  Output(bit, d_val);
  Output(smesh::ExCtrlMeshInput, d_bits);
  Output(bit, resp_rdy);

  void update();
  void reset();
};

class MesherMonitor : public Component {
  DECLARE_COMPONENT(MesherMonitor);

 public:
  MesherMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, req_rdy);
  Input(bit, a_rdy);
  Input(bit, b_rdy);
  Input(bit, d_rdy);
  Input(bit, resp_val);
  Input(smesh::MesherResp, resp_bits);
  InputArray(smesh::MesherTag, tags_in_progress, smesh::kRsExecuteEntries);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

MesherDriver::MesherDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(req_val, req_bits, a_val, a_bits, b_val, b_bits, d_val, d_bits)
      .writes(resp_rdy);
}

void MesherDriver::update() {
  smesh::ExCtrlMeshReq req{};
  req.total_rows = 4;
  req.tag.rs_tag_valid = 1;
  req.tag.rs_tag = 12;

  req_val = 1;
  req_bits = req;
  a_val = 1;
  a_bits = smesh::ExCtrlMeshInput{0x1111u, 1};
  b_val = 1;
  b_bits = smesh::ExCtrlMeshInput{0x2222u, 1};
  d_val = 1;
  d_bits = smesh::ExCtrlMeshInput{0x3333u, 1};
  resp_rdy = 1;
}

void MesherDriver::reset() {
  req_val.reset(0);
  req_bits.reset(smesh::ExCtrlMeshReq{});
  a_val.reset(0);
  a_bits.reset(smesh::ExCtrlMeshInput{});
  b_val.reset(0);
  b_bits.reset(smesh::ExCtrlMeshInput{});
  d_val.reset(0);
  d_bits.reset(smesh::ExCtrlMeshInput{});
  resp_rdy.reset(0);
}

MesherMonitor::MesherMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(req_rdy, a_rdy, b_rdy, d_rdy, resp_val, resp_bits, tags_in_progress);
}

void MesherMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  bool no_tags = true;
  for (std::size_t i = 0; i < smesh::kRsExecuteEntries; ++i) {
    no_tags = no_tags && tags_in_progress[i]->rs_tag_valid == 0;
  }

  passed_ =
      req_rdy != 0 &&
      a_rdy != 0 &&
      b_rdy != 0 &&
      d_rdy != 0 &&
      resp_val == 0 &&
      resp_bits->last == 0 &&
      no_tags;
  checked_ = true;
  done_ = true;
}

void MesherMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  MesherDriver driver("Driver");
  smesh::Mesher mesher("Mesher");
  MesherMonitor monitor("Monitor");

  mesher.req_val << driver.req_val;
  mesher.req_bits << driver.req_bits;
  mesher.a_val << driver.a_val;
  mesher.a_bits << driver.a_bits;
  mesher.b_val << driver.b_val;
  mesher.b_bits << driver.b_bits;
  mesher.d_val << driver.d_val;
  mesher.d_bits << driver.d_bits;
  mesher.resp_rdy << driver.resp_rdy;

  monitor.req_rdy << mesher.req_rdy;
  monitor.a_rdy << mesher.a_rdy;
  monitor.b_rdy << mesher.b_rdy;
  monitor.d_rdy << mesher.d_rdy;
  monitor.resp_val << mesher.resp_val;
  monitor.resp_bits << mesher.resp_bits;
  for (std::size_t i = 0; i < smesh::kRsExecuteEntries; ++i) {
    monitor.tags_in_progress[i] << mesher.tags_in_progress[i];
  }

  Clock clk;
  driver.clk << clk;
  mesher.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[MESHER] %s skeleton_boundary\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
