// **********************************************************************
// smesh/src/tb_ex_ctrl_mesh_cntl_pack.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
// Focused ExCtrlMeshCntlPack field-packaging test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlMeshCntlPack.hpp"

#include <cstdio>

class MeshCntlPackDriver : public Component {
  DECLARE_COMPONENT(MeshCntlPackDriver);

 public:
  MeshCntlPackDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Output(bit, perform_mul_pre);
  Output(bit, perform_single_mul);
  Output(bit, perform_single_preload);
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
  Output(bit, a_fire);
  Output(bit, b_fire);
  Output(bit, d_fire);
  Output(u32, a_unpadded_cols);
  Output(u32, b_unpadded_cols);
  Output(u32, d_unpadded_cols);
  Output(smesh::SmeshLocalAddr, c_addr);
  Output(u32, c_rows);
  Output(u32, c_cols);
  Output(bit, a_transpose);
  Output(bit, bd_transpose);
  Output(u32, total_rows);
  Output(bit, rs_tag_valid);
  Output(smesh::SmeshRsTag, rs_tag);
  Output(u8, dataflow);
  Output(bit, prop);
  Output(u32, shift);
  Output(bit, im2colling);
  Output(bit, first);

  void update();
  void reset();
};

class MeshCntlPackMonitor : public Component {
  DECLARE_COMPONENT(MeshCntlPackMonitor);

 public:
  MeshCntlPackMonitor(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(smesh::ExCtrlMeshCntl, cntl);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool checked_ = false;
  bool done_ = false;
  bool passed_ = false;
};

MeshCntlPackDriver::MeshCntlPackDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update)
      .writes(perform_mul_pre,
              perform_single_mul,
              perform_single_preload,
              a_bank,
              b_bank,
              d_bank,
              a_bank_acc,
              b_bank_acc)
      .writes(d_bank_acc,
              a_read_from_acc,
              b_read_from_acc,
              d_read_from_acc,
              a_garbage,
              b_garbage,
              d_garbage,
              accumulate_zeros)
      .writes(preload_zeros,
              a_fire,
              b_fire,
              d_fire,
              a_unpadded_cols,
              b_unpadded_cols,
              d_unpadded_cols,
              c_addr)
      .writes(c_rows,
              c_cols,
              a_transpose,
              bd_transpose,
              total_rows,
              rs_tag_valid,
              rs_tag,
              dataflow)
      .writes(prop, shift, im2colling, first);
}

void MeshCntlPackDriver::update() {
  perform_mul_pre = 0;
  perform_single_mul = 0;
  perform_single_preload = 1;
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
  b_garbage = 1;
  d_garbage = 0;
  accumulate_zeros = 1;
  preload_zeros = 0;
  a_fire = 1;
  b_fire = 0;
  d_fire = 1;
  a_unpadded_cols = 4;
  b_unpadded_cols = 5;
  d_unpadded_cols = 6;
  c_addr = smesh::makeAccAddr(9);
  c_rows = 7;
  c_cols = 8;
  a_transpose = 1;
  bd_transpose = 0;
  total_rows = 11;
  rs_tag_valid = 1;
  rs_tag = 42;
  dataflow = 1;
  prop = 1;
  shift = 3;
  im2colling = 0;
  first = 1;
}

void MeshCntlPackDriver::reset() {
  perform_mul_pre.reset(0);
  perform_single_mul.reset(0);
  perform_single_preload.reset(0);
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
  a_fire.reset(0);
  b_fire.reset(0);
  d_fire.reset(0);
  a_unpadded_cols.reset(0);
  b_unpadded_cols.reset(0);
  d_unpadded_cols.reset(0);
  c_addr.reset(smesh::SmeshLocalAddr{});
  c_rows.reset(0);
  c_cols.reset(0);
  a_transpose.reset(0);
  bd_transpose.reset(0);
  total_rows.reset(0);
  rs_tag_valid.reset(0);
  rs_tag.reset(0);
  dataflow.reset(0);
  prop.reset(0);
  shift.reset(0);
  im2colling.reset(0);
  first.reset(0);
}

MeshCntlPackMonitor::MeshCntlPackMonitor(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(cntl);
}

void MeshCntlPackMonitor::update() {
  if (Sim::state == Sim::SimResetting || checked_) {
    return;
  }

  const auto c = *cntl;
  passed_ =
      c.perform_single_preload != 0 &&
      c.a_bank == 1 &&
      c.b_bank == 2 &&
      c.d_bank == 3 &&
      c.b_bank_acc == 1 &&
      c.b_read_from_acc != 0 &&
      c.b_garbage != 0 &&
      c.accumulate_zeros != 0 &&
      c.a_fire != 0 &&
      c.b_fire == 0 &&
      c.d_fire != 0 &&
      c.a_unpadded_cols == 4 &&
      c.b_unpadded_cols == 5 &&
      c.d_unpadded_cols == 6 &&
      c.c_addr.raw == smesh::makeAccAddr(9).raw &&
      c.c_rows == 7 &&
      c.c_cols == 8 &&
      c.a_transpose != 0 &&
      c.total_rows == 11 &&
      c.rs_tag_valid != 0 &&
      c.rs_tag == 42 &&
      c.dataflow == 1 &&
      c.prop != 0 &&
      c.shift == 3 &&
      c.first != 0;
  checked_ = true;
  done_ = true;
}

void MeshCntlPackMonitor::reset() {
  checked_ = false;
  done_ = false;
  passed_ = false;
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  MeshCntlPackDriver driver("Driver");
  smesh::ExCtrlMeshCntlPack pack("MeshCntlPack");
  MeshCntlPackMonitor monitor("Monitor");

  pack.perform_mul_pre << driver.perform_mul_pre;
  pack.perform_single_mul << driver.perform_single_mul;
  pack.perform_single_preload << driver.perform_single_preload;
  pack.a_bank << driver.a_bank;
  pack.b_bank << driver.b_bank;
  pack.d_bank << driver.d_bank;
  pack.a_bank_acc << driver.a_bank_acc;
  pack.b_bank_acc << driver.b_bank_acc;
  pack.d_bank_acc << driver.d_bank_acc;
  pack.a_read_from_acc << driver.a_read_from_acc;
  pack.b_read_from_acc << driver.b_read_from_acc;
  pack.d_read_from_acc << driver.d_read_from_acc;
  pack.a_garbage << driver.a_garbage;
  pack.b_garbage << driver.b_garbage;
  pack.d_garbage << driver.d_garbage;
  pack.accumulate_zeros << driver.accumulate_zeros;
  pack.preload_zeros << driver.preload_zeros;
  pack.a_fire << driver.a_fire;
  pack.b_fire << driver.b_fire;
  pack.d_fire << driver.d_fire;
  pack.a_unpadded_cols << driver.a_unpadded_cols;
  pack.b_unpadded_cols << driver.b_unpadded_cols;
  pack.d_unpadded_cols << driver.d_unpadded_cols;
  pack.c_addr << driver.c_addr;
  pack.c_rows << driver.c_rows;
  pack.c_cols << driver.c_cols;
  pack.a_transpose << driver.a_transpose;
  pack.bd_transpose << driver.bd_transpose;
  pack.total_rows << driver.total_rows;
  pack.rs_tag_valid << driver.rs_tag_valid;
  pack.rs_tag << driver.rs_tag;
  pack.dataflow << driver.dataflow;
  pack.prop << driver.prop;
  pack.shift << driver.shift;
  pack.im2colling << driver.im2colling;
  pack.first << driver.first;

  monitor.cntl << pack.cntl;

  Clock clk;
  driver.clk << clk;
  pack.clk << clk;
  monitor.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 4 && !monitor.done(); ++i) {
    Sim::run();
  }

  const bool ok = monitor.done() && monitor.passed();
  std::printf("[EX_CTRL_MESH_CNTL_PACK] %s packaging\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
