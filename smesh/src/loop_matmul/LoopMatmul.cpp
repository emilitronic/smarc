// **********************************************************************
// smesh/src/loop_matmul/LoopMatmul.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026

#include "LoopMatmul.hpp"

#include "LoopMatmulCmdQueue.hpp"

#include "SmeshCommand.hpp"

namespace smesh {
namespace {

// Get function field  
SmeshFunct commandFunct(const SmeshQueuedCmd& issue) {
  return static_cast<SmeshFunct>(static_cast<std::uint32_t>(issue.cmd.funct));
}

// Check if a command is a LOOP_WS command or one of its setup commands.
bool isLoopCommand(SmeshFunct funct) {
  return funct == SmeshFunct::LoopWs ||
         (funct >= SmeshFunct::LoopWsBounds && funct <= SmeshFunct::LoopWsStridesDc);
}

} // namespace

LoopMatmul::LoopMatmul(std::string /*name*/, IMPL_CTOR) {
  cmd_queue_ = new LoopMatmulCmdQueue("CmdQueue");
  cmd_queue_->clk      << clk;
  cmd_queue_->cmd_val  << in_val;
  cmd_queue_->cmd_bits << in_bits;

  in_rdy     << cmd_queue_->cmd_rdy;

  head_val_  << cmd_queue_->head_val;
  head_bits_ << cmd_queue_->head_bits;

  cmd_queue_->head_rdy << head_rdy_;

  state_Q_ <= state_D_;
  UPDATE(updateDecision)
      .reads(state_Q_, head_val_, head_bits_, out_rdy)
      .writes(head_rdy_, out_val, out_bits, busy, head_loop_id, loop0, loop1);
  UPDATE(updateSlots)
      .reads(state_Q_, head_val_, head_bits_, head_rdy_)
      .writes(state_D_);
}

LoopMatmul::~LoopMatmul() { delete cmd_queue_; }

// read current loop-slot state and cmd queue head & compute outputs
void LoopMatmul::updateDecision() {
  const auto state = *state_Q_;
  const bool configured  = state.loops[0].configured == 1 || state.loops[1].configured == 1;
  const auto head_id     = static_cast<std::uint8_t>(state.head_id);
  const auto selected_id = state.loops[head_id].configured == 1 ? head_id ^ 1u : head_id;
  const auto selected    = state.loops[selected_id];

  // current status outputs
  busy         = bit(head_val_ == 1 || configured);
  head_loop_id = state.head_id;
  loop0        = state.loops[0];
  loop1        = state.loops[1];
  // whether ordinary cmd can pass through
  out_val      = 0;
  out_bits     = SmeshQueuedCmd{};
  // whether cmd queue may consume its head
  head_rdy_    = 0;

  if (head_val_ == 0) return;

  // if cmd queue head is valid decide if it can be consumed 
  // or forwared this cycle...
  const auto head = *head_bits_;
  // if LOOP_WS, consume if selected loop slot is not already configured
  if (isLoopCommand(commandFunct(head))) {
    head_rdy_ = bit(selected.configured == 0);  // cmd queue may remove cmd
  } else if (!configured) { // if just ordinary cmd and neither slot is configured
    out_val   = 1;
    out_bits  = head; // ... cmd passed through out_bits
    head_rdy_ = bit(out_rdy == 1); // queue removes it when downstream block asserts out_rdy
  }
}

// records an accepted LOOP_WS cmd
void LoopMatmul::updateSlots() {
  const auto current = *state_Q_;
  if (head_val_ == 0 || head_rdy_ == 0) {
    return;
  }

  const auto head  = *head_bits_;
  const auto funct = commandFunct(head);
  // ignore ordinary cmds, they are passed through in updateDecision() and do not affect loop slots
  if (!isLoopCommand(funct)) {
    return;
  }

  auto next = current;
  const auto head_id     = static_cast<std::uint8_t>(current.head_id);
  // select available slot
  const auto selected_id = current.loops[head_id].configured == 1 ? head_id ^ 1u : head_id;
  auto& slot             = next.loops[selected_id];

  const auto rs1 = static_cast<std::uint64_t>(head.cmd.rs1);
  const auto rs2 = static_cast<std::uint64_t>(head.cmd.rs2);
  // decode and apply loop cmd fields
  // 1) loop setup cmds fill in slot's bounds, addresses, and strides
  // 2) loop LOOP_WS cmd sets its options and makrs slot ast configured
  switch (funct) {
    case SmeshFunct::LoopWsBounds:
      slot.max_i = rs2 & 0xffffu;
      slot.max_j = (rs2 >> 16) & 0xffffu;
      slot.max_k = (rs2 >> 32) & 0xffffu;
      slot.pad_i = rs1 & 0xffffu;
      slot.pad_j = (rs1 >> 16) & 0xffffu;
      slot.pad_k = (rs1 >> 32) & 0xffffu;
      break;
    case SmeshFunct::LoopWsAddrsAb:
      slot.spad_only   = 0;
      slot.a_dram_addr = rs1;
      slot.b_dram_addr = rs2;
      break;
    case SmeshFunct::LoopWsAddrsDc:
      slot.spad_only   = 0;
      slot.d_dram_addr = rs1;
      slot.c_dram_addr = rs2;
      break;
    case SmeshFunct::LoopWsStridesAb:
      slot.a_dram_stride = rs1;
      slot.b_dram_stride = rs2;
      break;
    case SmeshFunct::LoopWsStridesDc:
      slot.d_dram_stride = rs1;
      slot.c_dram_stride = rs2;
      break;
    case SmeshFunct::LoopWs:
      slot.ex_accumulate = bit(rs1 & 1u);
      slot.full_c        = bit((rs1 >> 1) & 1u);
      slot.low_d         = bit((rs1 >> 2) & 1u);
      slot.act           = (rs1 >> 8) & 0x7u;
      slot.b_ex_spad_id  = (rs1 >> 16) & 0x3u;
      slot.a_ex_spad_id  = (rs1 >> 18) & 0x3u;
      slot.a_transpose   = bit(rs2 & 1u);
      slot.b_transpose   = bit((rs2 >> 1) & 1u);
      next.is_resadd     = bit((rs2 >> 2) & 1u);
      slot.lda_started   = bit((rs2 >> 3) & 1u);
      slot.ldb_started   = bit((rs2 >> 4) & 1u);
      slot.ldd_started   = bit((rs2 >> 5) & 1u);
      slot.ex_started    = bit((rs2 >> 6) & 1u);
      slot.st_started    = bit((rs2 >> 7) & 1u);
      slot.lda_completed = slot.lda_started;
      slot.ldb_completed = slot.ldb_started;
      slot.ldd_completed = slot.ldd_started;
      slot.ex_completed  = slot.ex_started;
      slot.st_completed  = slot.st_started;
      slot.inc_acc_addr  = bit((rs2 >> 8) & 1u);
      slot.spad_only     = bit((rs2 >> 9) & 1u);
      slot.c_spad_addr   = rs2 >> 32;
      slot.configured    = 1;
      break;
    default:
      break;
  }

  trace("loop_matmul: slot=%u funct=%u configured=%u",
        static_cast<unsigned>(selected_id), static_cast<unsigned>(funct),
        static_cast<unsigned>(slot.configured == 1));
  // write updated state      
  state_D_ = next;
}

void LoopMatmul::reset() {
  State state{};
  const auto half_spad = static_cast<std::uint32_t>(kSpRows / 2);
  const auto half_acc = static_cast<std::uint32_t>(kAccRows / 2);
  for (std::size_t i = 0; i < state.loops.size(); ++i) {
    state.loops[i].a_addr_start = i * half_spad;
    state.loops[i].b_addr_end = (i + 1) * half_spad;
    state.loops[i].resadd_addr_start = i * half_acc;
  }
  state_D_.reset(state);
  head_rdy_.reset(0);
  out_val.reset(0);
  out_bits.reset(SmeshQueuedCmd{});
  busy.reset(0);
  head_loop_id.reset(0);
  loop0.reset(state.loops[0]);
  loop1.reset(state.loops[1]);
}

} // namespace smesh
