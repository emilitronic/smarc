// **********************************************************************
// smesh/src/tb_ex_ctrl_mesh_cntl_queue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 29 2026
// Focused ExCtrlMeshCntlQueue enqueue/dequeue test.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrlMeshCntlQueue.hpp"

#include <cstdio>

class MeshCntlQueueDriver : public Component {
  DECLARE_COMPONENT(MeshCntlQueueDriver);

 public:
  MeshCntlQueueDriver(std::string name, COMPONENT_CTOR);

  Clock(clk);
  Input(bit, enq_rdy);
  Input(bit, deq_val);
  Input(smesh::ExCtrlMeshCntl, deq_bits);
  Input(smesh::ExCtrlMeshReq, mesh_req_bits);
  Output(bit, enq_val);
  Output(smesh::ExCtrlMeshCntl, enq_bits);
  Output(bit, deq_rdy);

  void update();
  void reset();

  bool done() const { return done_; }
  bool passed() const { return passed_; }

 private:
  bool sent_ = false;
  bool done_ = false;
  bool passed_ = false;
};

namespace {

smesh::ExCtrlMeshCntl makeExpected() {
  smesh::ExCtrlMeshCntl cntl{};
  cntl.perform_single_preload = 1;
  cntl.a_bank = 1;
  cntl.b_bank_acc = 1;
  cntl.d_read_from_acc = 1;
  cntl.a_fire = 1;
  cntl.d_fire = 1;
  cntl.c_addr = smesh::makeAccAddr(7);
  cntl.c_rows = 3;
  cntl.c_cols = 4;
  cntl.a_transpose = 1;
  cntl.total_rows = 5;
  cntl.rs_tag_valid = 1;
  cntl.rs_tag = 23;
  cntl.dataflow = 1;
  cntl.prop = 1;
  cntl.shift = 2;
  cntl.first = 1;
  return cntl;
}

bool matchesExpected(const smesh::ExCtrlMeshCntl& cntl) {
  const auto expected = makeExpected();
  return cntl.perform_single_preload == expected.perform_single_preload &&
         cntl.a_bank == expected.a_bank &&
         cntl.b_bank_acc == expected.b_bank_acc &&
         cntl.d_read_from_acc == expected.d_read_from_acc &&
         cntl.a_fire == expected.a_fire &&
         cntl.d_fire == expected.d_fire &&
         cntl.c_addr.raw == expected.c_addr.raw &&
         cntl.c_rows == expected.c_rows &&
         cntl.c_cols == expected.c_cols &&
         cntl.rs_tag_valid == expected.rs_tag_valid &&
         cntl.rs_tag == expected.rs_tag &&
         cntl.dataflow == expected.dataflow &&
         cntl.first == expected.first;
}

bool meshReqMatchesExpected(const smesh::ExCtrlMeshReq& req) {
  const auto expected = makeExpected();
  return req.pe_control.dataflow == expected.dataflow &&
         req.pe_control.propagate == expected.prop &&
         req.pe_control.shift == expected.shift &&
         req.a_transpose == expected.a_transpose &&
         req.bd_transpose == expected.bd_transpose &&
         req.total_rows == expected.total_rows &&
         req.tag.rs_tag_valid == expected.rs_tag_valid &&
         req.tag.rs_tag == expected.rs_tag &&
         req.tag.addr.raw == expected.c_addr.raw &&
         req.tag.rows == expected.c_rows &&
         req.tag.cols == expected.c_cols &&
         req.flush == 0;
}

} // namespace

MeshCntlQueueDriver::MeshCntlQueueDriver(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(enq_rdy, deq_val, deq_bits, mesh_req_bits).writes(enq_val, enq_bits, deq_rdy);
}

void MeshCntlQueueDriver::update() {
  enq_val = 0;
  enq_bits = smesh::ExCtrlMeshCntl{};
  deq_rdy = 0;

  if (!sent_ && enq_rdy != 0) {
    enq_val = 1;
    enq_bits = makeExpected();
    sent_ = true;
    return;
  }

  if (sent_ && deq_val != 0) {
    passed_ = matchesExpected(*deq_bits) && meshReqMatchesExpected(*mesh_req_bits);
    deq_rdy = 1;
    done_ = true;
  }
}

void MeshCntlQueueDriver::reset() {
  sent_ = false;
  done_ = false;
  passed_ = false;

  enq_val.reset(0);
  enq_bits.reset(smesh::ExCtrlMeshCntl{});
  deq_rdy.reset(0);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrlMeshCntlQueue queue("MeshCntlQueue");
  MeshCntlQueueDriver driver("Driver");

  queue.enq_val << driver.enq_val;
  queue.enq_bits << driver.enq_bits;
  driver.enq_rdy << queue.enq_rdy;
  driver.deq_val << queue.deq_val;
  driver.deq_bits << queue.deq_bits;
  driver.mesh_req_bits << queue.mesh_req_bits;
  queue.deq_rdy << driver.deq_rdy;

  Clock clk;
  queue.clk << clk;
  driver.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 8 && !driver.done(); ++i) {
    Sim::run();
  }

  const bool ok = driver.done() && driver.passed();
  std::printf("[EX_CTRL_MESH_CNTL_QUEUE] %s enqueue_dequeue\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
