// **********************************************************************
// smesh/tests/unit/rs/tb_rs_store_issue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include <cascade/Cascade.hpp>

#include "SmeshCommand.hpp"
#include "SmeshRS.hpp"
#include "StCtrlQueues.hpp"

#include <cstdio>

namespace {

class QueueConsumer : public Component {
  DECLARE_COMPONENT(QueueConsumer);

 public:
  QueueConsumer(std::string /*name*/, COMPONENT_CTOR) {
    UPDATE(update).writes(head_rdy);
  }

  Clock(clk);
  Output(bit, head_rdy);

  void update() {
    head_rdy = bit(cycle_ >= 6);
    ++cycle_;
  }

  void reset() {
    cycle_ = 0;
    head_rdy.reset(0);
  }

 private:
  unsigned cycle_ = 0;
};

smesh::SmeshCmd configStore() {
  smesh::SmeshCmd cmd{};
  cmd.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::Config);
  cmd.rs1 = smesh::packConfig(smesh::ConfigKind::Store);
  return cmd;
}

} // namespace

int main() {
  smesh::SmeshRS rs("RS");
  smesh::StCtrlCmdQueue queue("StoreQueue");
  QueueConsumer consumer("Consumer");
  Clock clk;
  rs.clk << clk;
  queue.clk << clk;
  consumer.clk << clk;
  queue.cmd_val << rs.issue_st_val;
  queue.cmd_bits << rs.issue_st_bits;
  rs.issue_st_rdy << queue.cmd_rdy;
  queue.head_rdy << consumer.head_rdy;
  rs.alloc_in.wireToZero();
  rs.completed.wireToZero();
  rs.issue_ld_rdy << consumer.head_rdy;
  rs.issue_ex_rdy << consumer.head_rdy;
  clk.generateClock();

  Sim::init();
  Sim::reset();
  rs.setStoreIssuePortEnabled(true);

  const auto cmd = configStore();
  bool ok = true;
  for (int i = 0; i < 2; ++i) {
    ok &= rs.allocate(cmd);
    Sim::run();
    ok &= !rs.storeEntry(0).valid;
  }

  ok &= rs.allocate(cmd);
  const auto blocked_tag = rs.storeEntry(0).rs_tag;
  for (int i = 0; i < 2; ++i) {
    Sim::run();
    ok &= rs.storeEntry(0).valid && !rs.storeEntry(0).issued &&
          rs.storeEntry(0).rs_tag == blocked_tag && queue.cmd_rdy == 0;
  }

  for (int i = 0; i < 8 && rs.storeEntry(0).valid; ++i) {
    Sim::run();
  }
  ok &= !rs.storeEntry(0).valid;

  smesh::SmeshCmd store{};
  store.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::Mvout);
  store.rs1 = 0x1000;
  store.rs2 = smesh::packLocal(smesh::makeSpAddr(0), {1, smesh::kDim});
  ok &= rs.allocate(store);
  const auto store_tag = rs.storeEntry(0).rs_tag;
  for (int i = 0; i < 8 && !rs.storeEntry(0).issued; ++i) {
    Sim::run();
  }
  ok &= rs.storeEntry(0).valid && rs.storeEntry(0).issued &&
        rs.storeEntry(0).rs_tag == store_tag;
  ok &= rs.complete(store_tag) && !rs.storeEntry(0).valid;

  std::printf("[RS_STORE_ISSUE] %s queue_acceptance\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
