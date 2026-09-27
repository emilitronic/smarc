// **********************************************************************
// smesh/src/loop_matmul/LoopMatmul.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026

#include "LoopMatmul.hpp"

#include "LoopMatmulCmdQueue.hpp"
#include "LoopMatmulLdABArb.hpp"
#include "LoopMatmulCmdArb.hpp"

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

  ld_a_ = new LoopMatmulLdA("LdA");
  ld_b_ = new LoopMatmulLdB("LdB");
  ld_d_ = new LoopMatmulLdD("LdD");
  ex_ = new LoopMatmulEx("Ex");
  st_c_ = new LoopMatmulStC("StC");
  st_c_spad_ = new LoopMatmulStCSpad("StCSpad");
  ld_ab_arb_ = new LoopMatmulLdABArb("LdABArb");
  cmd_arb_ = new LoopMatmulCmdArb("CmdArb");

  ld_a_->clk << clk;
  ld_b_->clk << clk;
  ld_d_->clk << clk;
  ex_->clk << clk;
  st_c_->clk << clk;
  st_c_spad_->clk << clk;
  ld_ab_arb_->clk << clk;
  cmd_arb_->clk << clk;

  ld_a_->req_val << generator_req_val_;
  ld_b_->req_val << generator_req_val_;
  ld_d_->req_val << generator_req_val_;
  ex_->req_val << generator_req_val_;
  st_c_->req_val << generator_req_val_;
  st_c_spad_->req_val << generator_req_val_;
  ld_a_->req_bits << ld_a_req_bits_;
  ld_b_->req_bits << ld_b_req_bits_;
  ld_d_->req_bits << ld_d_req_bits_;
  ex_->req_bits << ex_req_bits_;
  st_c_->req_bits << st_c_req_bits_;
  st_c_spad_->req_bits << st_c_spad_req_bits_;
  ld_a_->ld_utilization_at_limit << utilization_at_limit_;
  ld_b_->ld_utilization_at_limit << utilization_at_limit_;
  ld_d_->ld_utilization_at_limit << utilization_at_limit_;
  ex_->ex_utilization_at_limit << utilization_at_limit_;
  st_c_->st_utilization_at_limit << utilization_at_limit_;
  st_c_spad_->st_utilization_at_limit << utilization_at_limit_;

  ex_->ld_ka << ld_a_->k;
  ex_->ld_kb << ld_b_->k;
  ex_->ld_i << ld_a_->i;
  ex_->ld_j << ld_b_->j;
  ex_->lda_completed << progress_complete_;
  ex_->ldb_completed << progress_complete_;
  ex_->ldd_completed << progress_complete_;
  st_c_->ex_k << ex_->k;
  st_c_->ex_j << ex_->j;
  st_c_->ex_i << ex_->i;
  st_c_->ex_completed << progress_complete_;
  st_c_spad_->ex_k << ex_->k;
  st_c_spad_->ex_j << ex_->j;
  st_c_spad_->ex_i << ex_->i;
  st_c_spad_->ex_completed << progress_complete_;

  ld_ab_arb_->a_val << ld_a_->cmd_val;
  ld_ab_arb_->a_bits << ld_a_->cmd_bits;
  ld_ab_arb_->a_idle << ld_a_->idle;
  ld_ab_arb_->a_k << ld_a_->k;
  ld_ab_arb_->a_i << ld_a_->i;
  ld_ab_arb_->a_loop_id << ld_a_->loop_id;
  ld_a_->cmd_rdy << ld_ab_arb_->a_rdy;
  ld_ab_arb_->b_val << ld_b_->cmd_val;
  ld_ab_arb_->b_bits << ld_b_->cmd_bits;
  ld_ab_arb_->b_idle << ld_b_->idle;
  ld_ab_arb_->b_k << ld_b_->k;
  ld_ab_arb_->b_j << ld_b_->j;
  ld_ab_arb_->b_loop_id << ld_b_->loop_id;
  ld_b_->cmd_rdy << ld_ab_arb_->b_rdy;
  ld_ab_arb_->head_loop_id << head_loop_id;
  ld_ab_arb_->is_resadd << is_resadd_;

  cmd_arb_->st_c_val << st_c_->cmd_val;
  cmd_arb_->st_c_bits << st_c_->cmd_bits;
  st_c_->cmd_rdy << cmd_arb_->st_c_rdy;
  cmd_arb_->ex_val << ex_->cmd_val;
  cmd_arb_->ex_bits << ex_->cmd_bits;
  ex_->cmd_rdy << cmd_arb_->ex_rdy;
  cmd_arb_->ld_d_val << ld_d_->cmd_val;
  cmd_arb_->ld_d_bits << ld_d_->cmd_bits;
  ld_d_->cmd_rdy << cmd_arb_->ld_d_rdy;
  cmd_arb_->ld_ab_val << ld_ab_arb_->out_val;
  cmd_arb_->ld_ab_bits << ld_ab_arb_->out_bits;
  ld_ab_arb_->out_rdy << cmd_arb_->ld_ab_rdy;
  cmd_arb_->st_c_spad_val << st_c_spad_->cmd_val;
  cmd_arb_->st_c_spad_bits << st_c_spad_->cmd_bits;
  st_c_spad_->cmd_rdy << cmd_arb_->st_c_spad_rdy;
  cmd_arb_->out_rdy << out_rdy;
  unrolled_val_ << cmd_arb_->out_val;
  unrolled_bits_ << cmd_arb_->out_bits;

  state_Q_ <= state_D_;
  UPDATE(updateGeneratorInputs)
      .writes(generator_req_val_, ld_a_req_bits_, ld_b_req_bits_, ld_d_req_bits_,
              ex_req_bits_, st_c_req_bits_, st_c_spad_req_bits_, utilization_at_limit_)
      .writes(progress_complete_);
  UPDATE(updateStatus)
      .reads(state_Q_, head_val_)
      .writes(busy, head_loop_id, loop0, loop1, is_resadd_);
  UPDATE(updateDecision)
      .reads(state_Q_, head_val_, head_bits_, out_rdy, unrolled_val_, unrolled_bits_)
      .writes(head_rdy_, out_val, out_bits);
  UPDATE(updateSlots)
      .reads(state_Q_, head_val_, head_bits_, head_rdy_)
      .writes(state_D_);
}

LoopMatmul::~LoopMatmul() {
  delete cmd_arb_;
  delete ld_ab_arb_;
  delete st_c_spad_;
  delete st_c_;
  delete ex_;
  delete ld_d_;
  delete ld_b_;
  delete ld_a_;
  delete cmd_queue_;
}

void LoopMatmul::updateGeneratorInputs() {
  generator_req_val_ = 0;
  ld_a_req_bits_ = LoopMatmulLdAReq{};
  ld_b_req_bits_ = LoopMatmulLdBReq{};
  ld_d_req_bits_ = LoopMatmulLdDReq{};
  ex_req_bits_ = LoopMatmulExReq{};
  st_c_req_bits_ = LoopMatmulStCReq{};
  st_c_spad_req_bits_ = LoopMatmulStCSpadReq{};
  utilization_at_limit_ = 0;
  progress_complete_ = 0;
}

void LoopMatmul::updateStatus() {
  const auto state = *state_Q_;
  const bool configured = state.loops[0].configured == 1 || state.loops[1].configured == 1;
  busy = bit(head_val_ == 1 || configured);
  head_loop_id = state.head_id;
  loop0 = state.loops[0];
  loop1 = state.loops[1];
  is_resadd_ = state.is_resadd;
}

// read current loop-slot state and cmd queue head & compute outputs
void LoopMatmul::updateDecision() {
  const auto state = *state_Q_;
  const bool configured  = state.loops[0].configured == 1 || state.loops[1].configured == 1;
  const auto head_id     = static_cast<std::uint8_t>(state.head_id);
  const auto selected_id = state.loops[head_id].configured == 1 ? head_id ^ 1u : head_id;
  const auto selected    = state.loops[selected_id];

  out_val = configured ? *unrolled_val_ : bit(0);
  SmeshQueuedCmd output{};
  if (configured) {
    output.cmd = *unrolled_bits_;
    output.from_mmul_loop = 1;
  }
  out_bits = output;
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
  is_resadd_.reset(0);
  generator_req_val_.reset(0);
  ld_a_req_bits_.reset(LoopMatmulLdAReq{});
  ld_b_req_bits_.reset(LoopMatmulLdBReq{});
  ld_d_req_bits_.reset(LoopMatmulLdDReq{});
  ex_req_bits_.reset(LoopMatmulExReq{});
  st_c_req_bits_.reset(LoopMatmulStCReq{});
  st_c_spad_req_bits_.reset(LoopMatmulStCSpadReq{});
  utilization_at_limit_.reset(0);
  progress_complete_.reset(0);
}

} // namespace smesh
