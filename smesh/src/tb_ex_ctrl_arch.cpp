// **********************************************************************
// smesh/src/tb_ex_ctrl_arch.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 25 2026
// RS-like ExCtrl program driver.

#include <cascade/Cascade.hpp>
#include <descore/Parameter.hpp>

#include "ExCtrl.hpp"
#include "SmeshCommand.hpp"

#include <array>
#include <cstdio>

class FakeRsProgram : public Component {
  DECLARE_COMPONENT(FakeRsProgram);

 public:
  FakeRsProgram(std::string name, COMPONENT_CTOR);

  Clock(clk);
  FifoOutput(smesh::SmeshIssue, issue_out);
  Input(bit, completed_val);
  Input(smesh::SmeshRsTag, completed_bits);

  void updateIssue() {
    if (Sim::state == Sim::SimResetting || next_issue_ >= program_.size() || issue_out.full()) {
      return;
    }

    issue_out.push(program_[next_issue_]);
    ++next_issue_;
  }

  void updateCompleted() {
    if (Sim::state == Sim::SimResetting || completed_val == 0) {
      return;
    }

    const auto tag = *completed_bits;
    if (next_complete_ >= expected_tags_.size() || tag != expected_tags_[next_complete_]) {
      matched_ = false;
      done_ = true;
      return;
    }

    ++next_complete_;
    if (next_complete_ == expected_tags_.size()) {
      done_ = true;
    }
  }

  void reset() {
    next_issue_    = 0;
    next_complete_ = 0;
    done_          = false;
    matched_       = true;
  }

  bool done()    const { return done_; }
  bool matched() const { return matched_; }

 private:
  static smesh::SmeshIssue issue(smesh::SmeshRsTag tag, smesh::SmeshFunct funct, bool tag_valid = true) {
    smesh::SmeshIssue out{};
    out.rs_tag_valid = bit(tag_valid);
    out.rs_tag       = tag;
    out.cmd.funct    = static_cast<std::uint32_t>(funct);
    return out;
  }

  const std::array<smesh::SmeshIssue, 3> program_{{
      issue(11, smesh::SmeshFunct::Config),
      issue( 0, smesh::SmeshFunct::Preload, false),
      issue(12, smesh::SmeshFunct::Config),
  }};
  const std::array<smesh::SmeshRsTag, 2> expected_tags_{{11, 12}};

  std::size_t next_issue_    = 0;
  std::size_t next_complete_ = 0;
  bool done_                 = false;
  bool matched_              = true;
};

FakeRsProgram::FakeRsProgram(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateIssue).writes(issue_out);
  UPDATE(updateCompleted).reads(completed_val, completed_bits);
}

int main(int argc, char* argv[]) {
  descore::parseTraces(argc, argv);
  Parameter::parseCommandLine(argc, argv);
  Sim::parseDumps(argc, argv);

  smesh::ExCtrl ctrl("ExCtrl");
  FakeRsProgram fake_rs("FakeRS");

  ctrl.cmd_in            << fake_rs.issue_out;
  fake_rs.completed_val  << ctrl.completed_val;
  fake_rs.completed_bits << ctrl.completed_bits;
  ctrl.cmd_in.setDelay(1);

  Clock clk;
  ctrl.clk    << clk;
  fake_rs.clk << clk;
  clk.generateClock();

  Cascade::params.MaxResetIterations = 1;
  Sim::init();
  Sim::reset();
  for (int i = 0; i < 16 && !fake_rs.done(); ++i) {
    Sim::run();
  }

  const bool ok = fake_rs.done() && fake_rs.matched();
  std::printf("[EX_CTRL_ARCH] %s fake_rs_program\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
