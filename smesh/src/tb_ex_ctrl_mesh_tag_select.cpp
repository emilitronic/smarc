// **********************************************************************
// smesh/src/tb_ex_ctrl_mesh_tag_select.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Aug 16 2026
// Focused ExCtrlMeshTagSelect RS-tag selection test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlMeshTagSelect.hpp"

#include <cstdio>

namespace {

smesh::SmeshLocalAddr garbageAddr() {
  return smesh::SmeshLocalAddr{
      smesh::kLocalAddrIsAccMask |
      smesh::kLocalAddrAccumulateMask |
      smesh::kLocalAddrReadFullAccRowMask |
      smesh::kLocalAddrGarbageMask |
      smesh::kLocalAddrDataMask};
}

} // namespace

class MeshTagSelectDriver : public Component {
  DECLARE_COMPONENT(MeshTagSelectDriver);

 public:
  MeshTagSelectDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  OutputArray(bit, head_val, smesh::kExCtrlCmdWindow);
  OutputArray(smesh::SmeshIssue, head_bits, smesh::kExCtrlCmdWindow);
  Output(u8, preload_cmd_place);
  Output(bit, performing_single_mul);
  Output(smesh::SmeshLocalAddr, c_address_rs2);

  void update();
  void reset();

 private:
  int cycle_ = 0;
};

class MeshTagSelectMonitor : public Component {
  DECLARE_COMPONENT(MeshTagSelectMonitor);

 public:
  MeshTagSelectMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, mesh_rs_tag_valid);
  Input(smesh::SmeshRsTag, mesh_rs_tag);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  int cycle_ = 0;
  bool done_ = false;
  bool passed_ = true;
};

MeshTagSelectDriver::MeshTagSelectDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(head_val,
              head_bits,
              preload_cmd_place,
              performing_single_mul,
              c_address_rs2);
}

void MeshTagSelectDriver::update() {
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    head_val[i] = 1;
    smesh::SmeshIssue issue{};
    issue.rs_tag_valid = 1;
    issue.rs_tag = static_cast<smesh::SmeshRsTag>(10 + i);
    head_bits[i] = issue;
  }

  preload_cmd_place = 1;
  performing_single_mul = 0;
  c_address_rs2 = smesh::makeSpAddr(0);

  if (cycle_ == 1) {
    c_address_rs2 = garbageAddr();
  } else if (cycle_ == 2) {
    performing_single_mul = 1;
  } else if (cycle_ == 3) {
    preload_cmd_place = 2;
    head_val[2] = 0;
  }

  ++cycle_;
}

void MeshTagSelectDriver::reset() {
  cycle_ = 0;
  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    head_val[i].reset(0);
    head_bits[i].reset(smesh::SmeshIssue{});
  }
  preload_cmd_place.reset(0);
  performing_single_mul.reset(0);
  c_address_rs2.reset(smesh::makeSpAddr(0));
}

MeshTagSelectMonitor::MeshTagSelectMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(mesh_rs_tag_valid, mesh_rs_tag);
}

void MeshTagSelectMonitor::update() {
  if (Sim::state == Sim::SimResetting || done_) {
    return;
  }

  if (cycle_ == 0) {
    passed_ = passed_ && mesh_rs_tag_valid != 0 && mesh_rs_tag == 11;
  } else if (cycle_ == 1) {
    passed_ = passed_ && mesh_rs_tag_valid == 0 && mesh_rs_tag == 11;
  } else if (cycle_ == 2) {
    passed_ = passed_ && mesh_rs_tag_valid == 0 && mesh_rs_tag == 11;
  } else if (cycle_ == 3) {
    passed_ = passed_ && mesh_rs_tag_valid == 0 && mesh_rs_tag == 0;
    done_ = true;
  }

  ++cycle_;
}

void MeshTagSelectMonitor::reset() {
  cycle_ = 0;
  done_ = false;
  passed_ = true;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  MeshTagSelectDriver driver("Driver");
  smesh::ExCtrlMeshTagSelect select("MeshTagSelect");
  MeshTagSelectMonitor monitor("Monitor");

  for (std::size_t i = 0; i < smesh::kExCtrlCmdWindow; ++i) {
    select.head_val[i] << driver.head_val[i];
    select.head_bits[i] << driver.head_bits[i];
  }
  select.preload_cmd_place << driver.preload_cmd_place;
  select.performing_single_mul << driver.performing_single_mul;
  select.c_address_rs2 << driver.c_address_rs2;

  monitor.mesh_rs_tag_valid << select.mesh_rs_tag_valid;
  monitor.mesh_rs_tag << select.mesh_rs_tag;

  Clock clk;
  driver.clk << clk;
  select.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_MESH_TAG_SELECT] %s tag_select\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
