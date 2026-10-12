// **********************************************************************
// smesh/tests/unit/loop_conv/LoopConvRsCredits.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 12 2026
#pragma once

#include "LoopConv.hpp"
#include "SmeshCommand.hpp"
#include <algorithm>
#include <array>

// Test-only RS stand-in: remember accepted worker commands, then return their
// credits on later cycles. A delayed start deliberately fills the load counter.
class LoopConvRsCredits : public Component {
  DECLARE_COMPONENT(LoopConvRsCredits);
 public:
  LoopConvRsCredits(std::string /*name*/, unsigned drain_start, COMPONENT_CTOR)
      : drain_start_(drain_start) {
    pending_Q_ <= pending_D_;
    cycle_Q_ <= cycle_D_;
    UPDATE(updateReturns).reads(pending_Q_, cycle_Q_).writes(ld_completed, ex_completed, st_completed);
    UPDATE(updatePending).reads(pending_Q_, cycle_Q_, cmd_val, cmd_rdy, cmd_bits)
        .reads(ld_completed, ex_completed, st_completed).writes(pending_D_, cycle_D_);
  }
  Clock(clk);
  Input(bit, cmd_val);
  Input(bit, cmd_rdy);
  Input(smesh::SmeshQueuedCmd, cmd_bits);
  Output(u8, ld_completed);
  Output(u8, ex_completed);
  Output(u8, st_completed);
  void connect(smesh::LoopConv& loop, Clock& clock) {
    clk << clock;
    cmd_val << loop.out_val;
    cmd_rdy << loop.out_rdy;
    cmd_bits << loop.out_bits;
    loop.ld_completed << ld_completed;
    loop.ex_completed << ex_completed;
    loop.st_completed << st_completed;
  }
  void updateReturns() {
    const auto pending = *pending_Q_;
    const bool drain = cycle_Q_ >= drain_start_;
    ld_completed = drain ? std::min<unsigned>(pending[0], 2) : 0;
    ex_completed = drain ? std::min<unsigned>(pending[1], 2) : 0;
    st_completed = drain ? std::min<unsigned>(pending[2], 2) : 0;
  }
  void updatePending() {
    auto pending = *pending_Q_;
    pending[0] -= unsigned(*ld_completed);
    pending[1] -= unsigned(*ex_completed);
    pending[2] -= unsigned(*st_completed);
    const auto cmd = *cmd_bits;
    if (cmd_val == 1 && cmd_rdy == 1 && cmd.from_conv_loop == 1 && cmd.rs_tag_valid == 0) {
      const unsigned funct = unsigned(cmd.cmd.funct);
      const unsigned kind = funct == 0 ? std::uint64_t(cmd.cmd.rs1) & 3u : 0;
      const unsigned group = funct == 3 || (funct == 0 && kind == unsigned(smesh::ConfigKind::Store)) ? 2 :
          funct == 4 || funct == 5 || funct == 6 ||
          (funct == 0 && kind == unsigned(smesh::ConfigKind::Execute)) ? 1 : 0;
      ++pending[group];
    }
    pending_D_ = pending;
    cycle_D_ = unsigned(*cycle_Q_) + 1;
  }
  void reset() {
    pending_D_.reset({}); cycle_D_.reset(0);
    ld_completed.reset(0); ex_completed.reset(0); st_completed.reset(0);
  }
 private:
  const unsigned drain_start_;
  typedef std::array<unsigned, 3> Counts;
  Output(Counts, pending_Q_);
  Register(Counts, pending_D_);
  Output(u16, cycle_Q_);
  Register(u16, cycle_D_);
};
