// **********************************************************************
// smesh/src/tb_ex_ctrl_mesh_in_sel_pad.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
// Focused ExCtrlMeshInSelPad skeleton test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlMeshInSelPad.hpp"

#include <cstdio>

class MeshInSelPadDriver : public Component {
  DECLARE_COMPONENT(MeshInSelPadDriver);

 public:
  MeshInSelPadDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, cntl_val);
  Output(smesh::ExCtrlMeshCntl, cntl_bits);
  Output(u64, im2col_data);
  Output(bit, im2col_val);
  Output(bit, mesh_a_rdy);
  Output(bit, mesh_b_rdy);
  Output(bit, mesh_d_rdy);
  OutputArray(bit, spad_read_val, smesh::kSpBanks);
  OutputArray(bit, accum_read_val, smesh::kAccBanks);
  OutputArray(smesh::SpadReadResp, spad_read_data, smesh::kSpBanks);
  OutputArray(smesh::ExCtrlAccumReadResp, accum_read_data, smesh::kAccBanks);

  void update();
  void reset();
};

class MeshInSelPadMonitor : public Component {
  DECLARE_COMPONENT(MeshInSelPadMonitor);

 public:
  MeshInSelPadMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(smesh::ExCtrlMeshIn, mesh_a);
  Input(smesh::ExCtrlMeshIn, mesh_b);
  Input(smesh::ExCtrlMeshIn, mesh_d);
  Input(bit, mesh_a_val);
  Input(bit, mesh_b_val);
  Input(bit, mesh_d_val);
  Input(bit, mesh_a_fire);
  Input(bit, mesh_b_fire);
  Input(bit, mesh_d_fire);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

MeshInSelPadDriver::MeshInSelPadDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(cntl_val,
              cntl_bits)
      .writes(im2col_data,
              im2col_val)
      .writes(mesh_a_rdy,
              mesh_b_rdy,
              mesh_d_rdy,
              spad_read_val)
      .writes(accum_read_val, spad_read_data)
      .writes(accum_read_data);
}

void MeshInSelPadDriver::update() {
  cntl_val = 1;
  smesh::ExCtrlMeshCntl cntl{};
  cntl.a_bank = 1;
  cntl.b_bank = 2;
  cntl.d_bank = 3;
  cntl.a_bank_acc = 0;
  cntl.b_bank_acc = 1;
  cntl.d_bank_acc = 0;
  cntl.a_read_from_acc = 0;
  cntl.b_read_from_acc = 1;
  cntl.d_read_from_acc = 0;
  cntl.a_garbage = 0;
  cntl.b_garbage = 0;
  cntl.d_garbage = 0;
  cntl.accumulate_zeros = 0;
  cntl.preload_zeros = 1;
  cntl.im2colling = 1;
  cntl.a_unpadded_cols = 3;
  cntl.b_unpadded_cols = 2;
  cntl.d_unpadded_cols = 4;
  cntl.a_fire = 1;
  cntl.b_fire = 1;
  cntl.d_fire = 1;
  cntl_bits = cntl;
  im2col_data = 0x88776655u;
  im2col_val = 1;
  mesh_a_rdy = 1;
  mesh_b_rdy = 1;
  mesh_d_rdy = 1;

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    smesh::SpadReadResp resp{};
    resp.data = smesh::MeshInputRow{
        static_cast<smesh::Elem>(0x10 + bank),
        static_cast<smesh::Elem>(0x20 + bank),
        static_cast<smesh::Elem>(0x30 + bank),
        static_cast<smesh::Elem>(0x40 + bank)};
    resp.from_dma = 0;
    spad_read_val[bank] = 1;
    spad_read_data[bank] = resp;
  }

  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    smesh::ExCtrlAccumReadResp resp{};
    resp.data = smesh::MeshInputRow{
        static_cast<smesh::Elem>(0x01 + bank),
        static_cast<smesh::Elem>(0x20 + bank),
        0,
        0};
    resp.from_dma = 0;
    accum_read_val[bank] = 1;
    accum_read_data[bank] = resp;
  }
}

void MeshInSelPadDriver::reset() {
  cntl_val.reset(0);
  cntl_bits.reset(smesh::ExCtrlMeshCntl{});
  im2col_data.reset(0);
  im2col_val.reset(0);
  mesh_a_rdy.reset(0);
  mesh_b_rdy.reset(0);
  mesh_d_rdy.reset(0);

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_read_val[bank].reset(0);
    spad_read_data[bank].reset(smesh::SpadReadResp{});
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_val[bank].reset(0);
    accum_read_data[bank].reset(smesh::ExCtrlAccumReadResp{});
  }
}

MeshInSelPadMonitor::MeshInSelPadMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .reads(mesh_a, mesh_b, mesh_d, mesh_a_val, mesh_b_val, mesh_d_val)
      .reads(mesh_a_fire, mesh_b_fire, mesh_d_fire);
}

void MeshInSelPadMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  const auto a = *mesh_a;
  const auto b = *mesh_b;
  const auto d = *mesh_d;
  passed_ =
      a.data == smesh::MeshInputRow{static_cast<smesh::Elem>(0x55), static_cast<smesh::Elem>(0x66), static_cast<smesh::Elem>(0x77), 0} &&
      b.data == smesh::MeshInputRow{static_cast<smesh::Elem>(0x02), static_cast<smesh::Elem>(0x21), 0, 0} &&
      d.data == smesh::MeshInputRow{} &&
      mesh_a_val != 0 &&
      mesh_b_val != 0 &&
      mesh_d_val == 0 &&
      mesh_a_fire != 0 &&
      mesh_b_fire != 0 &&
      mesh_d_fire == 0;
  checked_ = true;
  done_ = true;
}

void MeshInSelPadMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  MeshInSelPadDriver driver("Driver");
  smesh::ExCtrlMeshInSelPad sel_pad("MeshInSelPad");
  MeshInSelPadMonitor monitor("Monitor");

  sel_pad.cntl_val << driver.cntl_val;
  sel_pad.cntl_bits << driver.cntl_bits;
  sel_pad.im2col_data << driver.im2col_data;
  sel_pad.im2col_val << driver.im2col_val;
  sel_pad.mesh_a_rdy << driver.mesh_a_rdy;
  sel_pad.mesh_b_rdy << driver.mesh_b_rdy;
  sel_pad.mesh_d_rdy << driver.mesh_d_rdy;
  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    sel_pad.spad_read_val[bank] << driver.spad_read_val[bank];
    sel_pad.spad_read_data[bank] << driver.spad_read_data[bank];
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    sel_pad.accum_read_val[bank] << driver.accum_read_val[bank];
    sel_pad.accum_read_data[bank] << driver.accum_read_data[bank];
  }

  monitor.mesh_a << sel_pad.mesh_a;
  monitor.mesh_b << sel_pad.mesh_b;
  monitor.mesh_d << sel_pad.mesh_d;
  monitor.mesh_a_val << sel_pad.mesh_a_val;
  monitor.mesh_b_val << sel_pad.mesh_b_val;
  monitor.mesh_d_val << sel_pad.mesh_d_val;
  monitor.mesh_a_fire << sel_pad.mesh_a_fire;
  monitor.mesh_b_fire << sel_pad.mesh_b_fire;
  monitor.mesh_d_fire << sel_pad.mesh_d_fire;

  Clock clk;
  driver.clk << clk;
  sel_pad.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_MESH_IN_SEL_PAD] %s mux_pad\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
