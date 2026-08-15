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
  Output(u32, a_bank);
  Output(u32, b_bank);
  Output(u32, d_bank);
  Output(u32, a_bank_acc);
  Output(u32, b_bank_acc);
  Output(u32, d_bank_acc);
  Output(bit, a_read_from_acc);
  Output(bit, b_read_from_acc);
  Output(bit, d_read_from_acc);
  Output(bit, a_garbage);
  Output(bit, b_garbage);
  Output(bit, d_garbage);
  Output(bit, accumulate_zeros);
  Output(bit, preload_zeros);
  Output(bit, im2colling);
  Output(u64, im2col_data);
  Output(bit, im2col_val);
  Output(u32, a_unpadded_cols);
  Output(u32, b_unpadded_cols);
  Output(u32, d_unpadded_cols);
  Output(bit, cntl_a_fire);
  Output(bit, cntl_b_fire);
  Output(bit, cntl_d_fire);
  Output(bit, mesh_a_rdy);
  Output(bit, mesh_b_rdy);
  Output(bit, mesh_d_rdy);
  OutputArray(bit, spad_read_val, smesh::kSpBanks);
  OutputArray(bit, accum_read_val, smesh::kAccBanks);
  OutputArray(smesh::SpadReadResp, spad_read_data, smesh::kSpBanks);
  OutputArray(smesh::AccumReadResp, accum_read_data, smesh::kAccBanks);

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
              a_bank,
              b_bank,
              d_bank,
              a_bank_acc,
              b_bank_acc,
              d_bank_acc,
              a_read_from_acc)
      .writes(b_read_from_acc,
              d_read_from_acc,
              a_garbage,
              b_garbage,
              d_garbage,
              accumulate_zeros,
              preload_zeros,
              im2colling)
      .writes(im2col_data,
              im2col_val,
              a_unpadded_cols,
              b_unpadded_cols,
              d_unpadded_cols,
              cntl_a_fire,
              cntl_b_fire,
              cntl_d_fire)
      .writes(mesh_a_rdy,
              mesh_b_rdy,
              mesh_d_rdy,
              spad_read_val)
      .writes(accum_read_val, spad_read_data)
      .writes(accum_read_data);
}

void MeshInSelPadDriver::update() {
  cntl_val = 1;
  a_bank = 1;
  b_bank = 2;
  d_bank = 3;
  a_bank_acc = 0;
  b_bank_acc = 1;
  d_bank_acc = 0;
  a_read_from_acc = 0;
  b_read_from_acc = 1;
  d_read_from_acc = 0;
  a_garbage = 0;
  b_garbage = 0;
  d_garbage = 0;
  accumulate_zeros = 0;
  preload_zeros = 1;
  im2colling = 1;
  im2col_data = 0x88776655u;
  im2col_val = 1;
  a_unpadded_cols = 3;
  b_unpadded_cols = 2;
  d_unpadded_cols = 4;
  cntl_a_fire = 1;
  cntl_b_fire = 1;
  cntl_d_fire = 1;
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
    smesh::AccumReadResp resp{};
    resp.data = smesh::MeshAccumRow{
        static_cast<smesh::Acc>(0x2000u + bank),
        0,
        0,
        0};
    resp.from_dma = 0;
    accum_read_val[bank] = 1;
    accum_read_data[bank] = resp;
  }
}

void MeshInSelPadDriver::reset() {
  cntl_val.reset(0);
  a_bank.reset(0);
  b_bank.reset(0);
  d_bank.reset(0);
  a_bank_acc.reset(0);
  b_bank_acc.reset(0);
  d_bank_acc.reset(0);
  a_read_from_acc.reset(0);
  b_read_from_acc.reset(0);
  d_read_from_acc.reset(0);
  a_garbage.reset(0);
  b_garbage.reset(0);
  d_garbage.reset(0);
  accumulate_zeros.reset(0);
  preload_zeros.reset(0);
  im2colling.reset(0);
  im2col_data.reset(0);
  im2col_val.reset(0);
  a_unpadded_cols.reset(0);
  b_unpadded_cols.reset(0);
  d_unpadded_cols.reset(0);
  cntl_a_fire.reset(0);
  cntl_b_fire.reset(0);
  cntl_d_fire.reset(0);
  mesh_a_rdy.reset(0);
  mesh_b_rdy.reset(0);
  mesh_d_rdy.reset(0);

  for (std::size_t bank = 0; bank < smesh::kSpBanks; ++bank) {
    spad_read_val[bank].reset(0);
    spad_read_data[bank].reset(smesh::SpadReadResp{});
  }
  for (std::size_t bank = 0; bank < smesh::kAccBanks; ++bank) {
    accum_read_val[bank].reset(0);
    accum_read_data[bank].reset(smesh::AccumReadResp{});
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
      b.data == smesh::MeshInputRow{static_cast<smesh::Elem>(0x01), 0, 0, 0} &&
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
  sel_pad.a_bank << driver.a_bank;
  sel_pad.b_bank << driver.b_bank;
  sel_pad.d_bank << driver.d_bank;
  sel_pad.a_bank_acc << driver.a_bank_acc;
  sel_pad.b_bank_acc << driver.b_bank_acc;
  sel_pad.d_bank_acc << driver.d_bank_acc;
  sel_pad.a_read_from_acc << driver.a_read_from_acc;
  sel_pad.b_read_from_acc << driver.b_read_from_acc;
  sel_pad.d_read_from_acc << driver.d_read_from_acc;
  sel_pad.a_garbage << driver.a_garbage;
  sel_pad.b_garbage << driver.b_garbage;
  sel_pad.d_garbage << driver.d_garbage;
  sel_pad.accumulate_zeros << driver.accumulate_zeros;
  sel_pad.preload_zeros << driver.preload_zeros;
  sel_pad.im2colling << driver.im2colling;
  sel_pad.im2col_data << driver.im2col_data;
  sel_pad.im2col_val << driver.im2col_val;
  sel_pad.a_unpadded_cols << driver.a_unpadded_cols;
  sel_pad.b_unpadded_cols << driver.b_unpadded_cols;
  sel_pad.d_unpadded_cols << driver.d_unpadded_cols;
  sel_pad.cntl_a_fire << driver.cntl_a_fire;
  sel_pad.cntl_b_fire << driver.cntl_b_fire;
  sel_pad.cntl_d_fire << driver.cntl_d_fire;
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
