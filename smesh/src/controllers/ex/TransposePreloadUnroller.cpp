// **********************************************************************
// smesh/src/controllers/ex/TransposePreloadUnroller.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 24 2026

#include "TransposePreloadUnroller.hpp"
#include "SmeshCommand.hpp"

namespace smesh {

namespace {
bool isFunct(const SmeshIssue& issue, SmeshFunct funct) {
  return issue.cmd.funct == static_cast<std::uint32_t>(funct);
}

// Replace the local address only; retain the operand's row/column dimensions.
std::uint64_t garbageOperand(std::uint64_t operand) {
  return (operand & ~kLocalAddrMask) | kLocalAddrMask;
}
} // namespace

TransposePreloadUnroller::TransposePreloadUnroller(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateReady).reads(state_Q_).writes(in_rdy);
  UPDATE(updateOutput).reads(state_Q_).writes(out_val, out_bits);
  UPDATE(updateState).reads(state_Q_, in_val, in_rdy, in_bits, out_val, out_rdy, out_bits)
                     .writes(state_D_);
}

// Accept a new RS-issued command only when a queue entry is already free.
void TransposePreloadUnroller::updateReady() {
  in_rdy = bit(state_Q_->count < 2);
}

// Present the next command to ExCtrl's main queue; stalls leave it unchanged.
void TransposePreloadUnroller::updateOutput() {
  const auto q = *state_Q_;
  out_val = 0;
  out_bits = SmeshIssue{};
  if (q.count == 0) return;

  auto command = q.entries[0];
  const auto phase = static_cast<Phase>(static_cast<unsigned>(q.phase));
  if (phase == Phase::FirstCompute) {
    command = q.entries[1];
    command.cmd.funct = static_cast<std::uint32_t>(SmeshFunct::ComputeStay);
    command.cmd.rs1 = garbageOperand(command.cmd.rs1);
    command.cmd.rs2 = garbageOperand(command.cmd.rs2);
    command.rs_tag_valid = 0;
  } else if (phase == Phase::SecondPreload) {
    command.cmd.rs1 = garbageOperand(command.cmd.rs1);
  } else if (q.transpose_ws == 1 && isFunct(command, SmeshFunct::Preload)) {
    // A transposed PRELOAD must wait until its following command is known.
    if (q.count < 2) return;
    if (isFunct(q.entries[1], SmeshFunct::ComputeFlip)) {
      command.cmd.rs2 = garbageOperand(command.cmd.rs2);
      command.rs_tag_valid = 0;
    }
  }
  out_val = 1;
  out_bits = command;
}

// Advance the expansion on output fire, then append an accepted input command.
void TransposePreloadUnroller::updateState() {
  const auto q = *state_Q_;
  auto next = q;
  const bool push = in_val == 1 && in_rdy == 1;
  const bool fire = out_val == 1 && out_rdy == 1;
  if (!push && !fire) return;

  if (fire) {
    const auto phase = static_cast<Phase>(static_cast<unsigned>(q.phase));
    const auto& head = q.entries[0];
    bool pop = true;
    if (phase == Phase::FirstCompute) {
      next.phase = static_cast<std::uint8_t>(Phase::SecondPreload);
      pop = false;
    } else if (phase == Phase::SecondPreload) {
      next.phase = static_cast<std::uint8_t>(Phase::Idle);
    } else if (q.transpose_ws == 1 && q.count == 2 &&
               isFunct(head, SmeshFunct::Preload) &&
               isFunct(q.entries[1], SmeshFunct::ComputeFlip)) {
      next.phase = static_cast<std::uint8_t>(Phase::FirstCompute);
      pop = false;
    } else if (isFunct(head, SmeshFunct::Config) &&
               (static_cast<std::uint64_t>(head.cmd.rs1) & 0x3u) ==
                   static_cast<std::uint32_t>(ConfigKind::Execute) &&
               !unpackConfigExSetOnlyStrides(head.cmd.rs1)) {
      next.transpose_ws = bit(unpackConfigExDataflow(head.cmd.rs1) == kExDataflowWS &&
                              unpackConfigExBTranspose(head.cmd.rs1));
    }
    if (pop) {
      next.entries[0] = q.count == 2 ? q.entries[1] : SmeshIssue{};
      next.entries[1] = SmeshIssue{};
      next.count = static_cast<unsigned>(q.count) - 1;
    }
    trace("transpose_unroll: output phase=%u funct=%u pop=%u tag_valid=%u\n",
          static_cast<unsigned>(q.phase), static_cast<unsigned>(out_bits->cmd.funct),
          unsigned(pop), static_cast<unsigned>(out_bits->rs_tag_valid));
  }
  if (push) {
    next.entries[static_cast<unsigned>(next.count)] = *in_bits;
    next.count = static_cast<unsigned>(next.count) + 1;
    trace("transpose_unroll: input funct=%u tag=%u\n",
          static_cast<unsigned>(in_bits->cmd.funct), static_cast<unsigned>(in_bits->rs_tag));
  }
  state_D_ = next;
}

void TransposePreloadUnroller::reset() {
  state_D_.reset(State{});
  in_rdy.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshIssue{});
}

} // namespace smesh
