// **********************************************************************
// smesh/tests/unit/rs/tb_rs_execute_issue.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 25 2026
// Focused test of RS execute issue handshake with backpressure from consumer.
// Keep Execute ready low before accepting a command.

#include <cascade/Cascade.hpp>

#include "SmeshCommand.hpp"
#include "SmeshRS.hpp"

#include <cstdio>

namespace {

class IssueReady : public Component {
  DECLARE_COMPONENT(IssueReady);

 public:
  IssueReady(std::string /*name*/, COMPONENT_CTOR) {
    UPDATE(update).writes(ready);
  }

  Clock(clk);
  Output(bit, ready);

  void update() { ready = bit(cycle_++ >= 3); }
  void reset() {
    cycle_ = 0;
    ready.reset(0);
  }

 private:
  unsigned cycle_ = 0;
};

} // namespace

int main() {
  smesh::SmeshRS rs("RS");
  IssueReady consumer("Consumer");
  Clock clk;
  rs.clk << clk;
  consumer.clk << clk;
  rs.alloc_in.wireToZero();
  rs.completed.wireToZero();
  rs.issue_ld_rdy << consumer.ready;
  rs.issue_st_rdy << consumer.ready;
  rs.issue_ex_rdy << consumer.ready;
  clk.generateClock();

  Sim::init();
  Sim::reset();
  rs.setExecuteIssuePortEnabled(true);

  smesh::SmeshCmd cmd{};
  cmd.funct = static_cast<std::uint32_t>(smesh::SmeshFunct::ComputeStay);
  bool ok = rs.allocate(cmd);
  const auto tag = rs.executeEntry(0).rs_tag;

  for (int i = 0; i < 3; ++i) {
    Sim::run();
    ok &= rs.issue_ex_val == 1 && rs.issue_ex_bits->rs_tag == tag &&
          !rs.executeEntry(0).issued;
  }

  for (int i = 0; i < 4 && !rs.executeEntry(0).issued; ++i) {
    Sim::run();
  }
  ok &= rs.executeEntry(0).valid && rs.executeEntry(0).issued;
  ok &= rs.complete(tag) && rs.empty();

  std::printf("[RS_EXECUTE_ISSUE] %s ready_handshake\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
