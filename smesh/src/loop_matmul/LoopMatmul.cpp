// **********************************************************************
// smesh/src/loop_matmul/LoopMatmul.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 26 2026

#include "LoopMatmul.hpp"

#include "LoopMatmulCmdQueue.hpp"
#include "LoopMatmulLdABArb.hpp"
#include "LoopMatmulCmdArb.hpp"
#include "LoopMatmulLdUtilization.hpp"

#include "SmeshCommand.hpp"

#include <cassert>

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

bool allCompleted(const LoopMatmulSlot& slot) {
  return slot.lda_completed == 1 && slot.ldb_completed == 1 &&
         slot.ldd_completed == 1 && slot.ex_completed == 1 &&
         slot.st_completed == 1;
}

void clearLoopProgress(LoopMatmulSlot& slot) {
  slot.configured = 0;
  slot.running = 0;
  slot.lda_started = 0;
  slot.ldb_started = 0;
  slot.ldd_started = 0;
  slot.ex_started = 0;
  slot.st_started = 0;
  slot.lda_completed = 0;
  slot.ldb_completed = 0;
  slot.ldd_completed = 0;
  slot.ex_completed = 0;
  slot.st_completed = 0;
  slot.spad_only = 0;
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

  ld_a_           = new LoopMatmulLdA("LdA");
  ld_b_           = new LoopMatmulLdB("LdB");
  ld_d_           = new LoopMatmulLdD("LdD");
  ex_             = new LoopMatmulEx("Ex");
  st_c_           = new LoopMatmulStC("StC");
  st_c_spad_      = new LoopMatmulStCSpad("StCSpad");
  ld_ab_arb_      = new LoopMatmulLdABArb("LdABArb");
  cmd_arb_        = new LoopMatmulCmdArb("CmdArb");
  ld_utilization_ = new LoopMatmulLdUtilization("LdUtilization");

  ld_a_->clk           << clk;
  ld_b_->clk           << clk;
  ld_d_->clk           << clk;
  ex_->clk             << clk;
  st_c_->clk           << clk;
  st_c_spad_->clk      << clk;
  ld_ab_arb_->clk      << clk;
  cmd_arb_->clk        << clk;
  ld_utilization_->clk << clk;

  ld_a_->req_val  << ld_a_req_val_;
  ld_a_req_rdy_   << ld_a_->req_rdy;
  ld_a_->req_bits << ld_a_req_bits_;

  ld_b_->req_val  << ld_b_req_val_;
  ld_b_req_rdy_   << ld_b_->req_rdy;
  ld_b_->req_bits << ld_b_req_bits_;

  ld_d_->req_val  << ld_d_req_val_;
  ld_d_req_rdy_   << ld_d_->req_rdy;
  ld_d_->req_bits << ld_d_req_bits_;

  ex_->req_val    << ex_req_val_;
  ex_req_rdy_     << ex_->req_rdy;
  ex_->req_bits   << ex_req_bits_;

  st_c_->req_val  << st_c_req_val_;
  st_c_req_rdy_   << st_c_->req_rdy;
  st_c_->req_bits << st_c_req_bits_;

  st_c_spad_->req_val  << st_c_spad_req_val_;
  st_c_spad_req_rdy_   << st_c_spad_->req_rdy;
  st_c_spad_->req_bits << st_c_spad_req_bits_;

  ld_a_->ld_utilization_at_limit << ld_utilization_->ld_utilization_at_limit;
  ld_b_->ld_utilization_at_limit << ld_utilization_->ld_utilization_at_limit;
  ld_d_->ld_utilization_at_limit << ld_utilization_->ld_utilization_at_limit;

  ex_->ex_utilization_at_limit        << ex_utilization_at_limit_;
  st_c_->st_utilization_at_limit      << st_utilization_at_limit_;
  st_c_spad_->st_utilization_at_limit << st_utilization_at_limit_;

  ld_utilization_->lda_cmd_fire << ld_a_cmd_fire_;
  ld_utilization_->ldb_cmd_fire << ld_b_cmd_fire_;
  ld_utilization_->ldd_cmd_fire << ld_d_cmd_fire_;
  ld_utilization_->ld_completed << ld_completed;

  ex_->ld_ka << ld_a_->k;
  ex_->ld_kb << ld_b_->k;
  ex_->ld_i  << ld_a_->i;
  ex_->ld_j  << ld_b_->j;
  ex_->lda_completed << lda_complete_to_ex_;
  ex_->ldb_completed << ldb_complete_to_ex_;
  ex_->ldd_completed << ldd_complete_to_ex_;
  st_c_->ex_k << ex_->k;
  st_c_->ex_j << ex_->j;
  st_c_->ex_i << ex_->i;
  st_c_->ex_completed << ex_complete_to_st_c_;
  st_c_spad_->ex_k << ex_->k;
  st_c_spad_->ex_j << ex_->j;
  st_c_spad_->ex_i << ex_->i;
  st_c_spad_->ex_completed << ex_complete_to_st_c_spad_;

  ld_a_idle_         << ld_a_->idle;
  ld_a_loop_id_      << ld_a_->loop_id;
  ld_b_idle_         << ld_b_->idle;
  ld_b_loop_id_      << ld_b_->loop_id;
  ld_d_idle_         << ld_d_->idle;
  ld_d_loop_id_      << ld_d_->loop_id;
  ex_idle_           << ex_->idle;
  ex_loop_id_        << ex_->loop_id;
  st_c_idle_         << st_c_->idle;
  st_c_loop_id_      << st_c_->loop_id;
  st_c_spad_idle_    << st_c_spad_->idle;
  st_c_spad_loop_id_ << st_c_spad_->loop_id;
  ld_a_cmd_val_      << ld_a_->cmd_val;
  ld_a_cmd_rdy_      << ld_a_->cmd_rdy;
  ld_b_cmd_val_      << ld_b_->cmd_val;
  ld_b_cmd_rdy_      << ld_b_->cmd_rdy;
  ld_d_cmd_val_      << ld_d_->cmd_val;
  ld_d_cmd_rdy_      << ld_d_->cmd_rdy;
  ex_cmd_val_        << ex_->cmd_val;
  ex_cmd_rdy_        << ex_->cmd_rdy;
  st_c_cmd_val_      << st_c_->cmd_val;
  st_c_cmd_rdy_      << st_c_->cmd_rdy;
  st_c_spad_cmd_val_ << st_c_spad_->cmd_val;
  st_c_spad_cmd_rdy_ << st_c_spad_->cmd_rdy;

  ld_ab_arb_->a_val     << ld_a_->cmd_val;
  ld_ab_arb_->a_bits    << ld_a_->cmd_bits;
  ld_ab_arb_->a_idle    << ld_a_->idle;
  ld_ab_arb_->a_k       << ld_a_->k;
  ld_ab_arb_->a_i       << ld_a_->i;
  ld_ab_arb_->a_loop_id << ld_a_->loop_id;
  ld_a_->cmd_rdy        << ld_ab_arb_->a_rdy;
  ld_ab_arb_->b_val     << ld_b_->cmd_val;
  ld_ab_arb_->b_bits    << ld_b_->cmd_bits;
  ld_ab_arb_->b_idle    << ld_b_->idle;
  ld_ab_arb_->b_k       << ld_b_->k;
  ld_ab_arb_->b_j       << ld_b_->j;
  ld_ab_arb_->b_loop_id << ld_b_->loop_id;
  ld_b_->cmd_rdy        << ld_ab_arb_->b_rdy;
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
      .reads(state_Q_)
      .writes(ld_a_req_val_, ld_a_req_bits_, ld_b_req_val_, ld_b_req_bits_,
              ld_d_req_val_, ld_d_req_bits_, ex_req_val_, ex_req_bits_)
      .writes(st_c_req_val_, st_c_req_bits_, st_c_spad_req_val_,
              st_c_spad_req_bits_);
  UPDATE(updateCapacity).reads(state_Q_)
      .writes(st_utilization_at_limit_, ex_utilization_at_limit_);
  UPDATE(updateCommandFires)
      .reads(ld_a_cmd_val_, ld_a_cmd_rdy_, ld_b_cmd_val_, ld_b_cmd_rdy_,
             ld_d_cmd_val_, ld_d_cmd_rdy_, ex_cmd_val_, ex_cmd_rdy_)
      .reads(st_c_cmd_val_, st_c_cmd_rdy_, st_c_spad_cmd_val_, st_c_spad_cmd_rdy_)
      .writes(ld_a_cmd_fire_, ld_b_cmd_fire_, ld_d_cmd_fire_,
              ex_cmd_fire_, st_c_cmd_fire_, st_c_spad_cmd_fire_);
  UPDATE(updateProgress)
      .reads(state_Q_, ld_a_idle_, ld_a_loop_id_, ld_b_idle_, ld_b_loop_id_,
             ld_d_idle_, ld_d_loop_id_, ex_idle_)
      .reads(ex_loop_id_, st_c_loop_id_, st_c_spad_loop_id_)
      .writes(lda_complete_to_ex_, ldb_complete_to_ex_, ldd_complete_to_ex_,
              ex_complete_to_st_c_, ex_complete_to_st_c_spad_);
  UPDATE(updateStatus)
      .reads(state_Q_, head_val_)
      .writes(busy, head_loop_id, loop0, loop1, is_resadd_, completed0, completed1);
  UPDATE(updateDecision)
      .reads(state_Q_, head_val_, head_bits_, out_rdy, unrolled_val_, unrolled_bits_)
      .writes(head_rdy_, out_val, out_bits);
  UPDATE(updateSlots)
      .reads(state_Q_, head_val_, head_bits_, head_rdy_, ld_a_req_val_,
             ld_a_req_rdy_, ld_b_req_val_, ld_b_req_rdy_)
      .reads(ld_d_req_val_, ld_d_req_rdy_, ex_req_val_, ex_req_rdy_,
             st_c_req_val_, st_c_req_rdy_, st_c_spad_req_val_, st_c_spad_req_rdy_)
      .reads(ld_a_idle_, ld_a_loop_id_, ld_b_idle_, ld_b_loop_id_,
             ld_d_idle_, ld_d_loop_id_, ex_idle_, ex_loop_id_)
      .reads(st_c_idle_, st_c_loop_id_, st_c_spad_idle_, st_c_spad_loop_id_)
      .reads(st_completed, ex_completed, st_c_cmd_fire_, st_c_spad_cmd_fire_,
             ex_cmd_fire_, completed0, completed1)
      .writes(state_D_);
}

LoopMatmul::~LoopMatmul() {
  delete ld_utilization_;
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
  const auto state = *state_Q_;
  const auto head  = static_cast<std::uint8_t>(state.head_id);
  const auto tail  = static_cast<std::uint8_t>(head ^ 1u);
  const auto a_id  = state.loops[head].lda_started == 1 ? tail : head;
  const auto b_id  = state.loops[head].ldb_started == 1 ? tail : head;
  const auto d_id  = state.loops[head].ldd_started == 1 ? tail : head;
  const auto ex_id = state.loops[head].ex_started == 1 ? tail : head;
  const auto st_id = state.loops[head].st_started == 1 ? tail : head;
  const auto& a    = state.loops[a_id];
  const auto& b    = state.loops[b_id];
  const auto& d    = state.loops[d_id];
  const auto& ex   = state.loops[ex_id];
  const auto& st   = state.loops[st_id];
  const auto half_spad = static_cast<std::uint32_t>(kSpRows / 2);

  LoopMatmulLdAReq a_req{};
  a_req.max_k = state.is_resadd == 1 ? a.max_j : a.max_k;
  a_req.max_i = a.max_i;
  a_req.pad_k = state.is_resadd == 1 ? a.pad_j : a.pad_k;
  a_req.pad_i = a.pad_i;
  a_req.dram_addr = a.a_dram_addr;
  a_req.dram_stride = a.a_dram_stride;
  a_req.transpose = a.a_transpose;
  a_req.addr_start = state.is_resadd == 1 ? a.resadd_addr_start :
      a.a_ex_spad_id == 0 ? a.a_addr_start : (a.a_ex_spad_id - 1) * half_spad;
  a_req.loop_id = a_id;
  a_req.is_resadd = state.is_resadd;
  ld_a_req_bits_ = a_req;
  ld_a_req_val_ = bit(a.configured == 1 && a.lda_started == 0);

  LoopMatmulLdBReq b_req{};
  b_req.max_j = b.max_j;
  b_req.max_k = state.is_resadd == 1 ? b.max_i : b.max_k;
  b_req.pad_j = b.pad_j;
  b_req.pad_k = state.is_resadd == 1 ? b.pad_i : b.pad_k;
  b_req.dram_addr = b.b_dram_addr;
  b_req.dram_stride = b.b_dram_stride;
  b_req.transpose = b.b_transpose;
  b_req.addr_end = state.is_resadd == 1 ? b.resadd_addr_start :
      b.b_ex_spad_id == 0 ? b.b_addr_end : b.b_ex_spad_id * half_spad;
  b_req.loop_id = b_id;
  b_req.is_resadd = state.is_resadd;
  ld_b_req_bits_ = b_req;
  ld_b_req_val_ = bit(b.configured == 1 && b.ldb_started == 0);

  LoopMatmulLdDReq d_req{};
  d_req.max_j = d.max_j;
  d_req.max_i = d.max_i;
  d_req.pad_j = d.pad_j;
  d_req.pad_i = d.pad_i;
  d_req.dram_addr = d.d_dram_addr;
  d_req.dram_stride = d.d_dram_stride;
  d_req.low_d = d.low_d;
  d_req.addr_start = static_cast<std::uint32_t>(state.ld_d_addr_start);
  d_req.loop_id = d_id;
  ld_d_req_bits_ = d_req;
  ld_d_req_val_ = bit(d.configured == 1 && d.ldd_started == 0);

  LoopMatmulExReq ex_req{};
  ex_req.max_j = ex.max_j;
  ex_req.max_k = ex.max_k;
  ex_req.max_i = ex.max_i;
  ex_req.pad_j = ex.pad_j;
  ex_req.pad_k = ex.pad_k;
  ex_req.pad_i = ex.pad_i;
  ex_req.accumulate = ex.ex_accumulate;
  ex_req.a_addr_start = ex.spad_only == 1 || ex.a_ex_spad_id == 0 ?
      ex.a_addr_start : (ex.a_ex_spad_id - 1) * half_spad;
  ex_req.b_addr_end = ex.spad_only == 1 || ex.b_ex_spad_id == 0 ?
      ex.b_addr_end : ex.b_ex_spad_id * half_spad;
  ex_req.a_transpose = ex.a_transpose;
  ex_req.b_transpose = ex.b_transpose;
  ex_req.c_addr_start = static_cast<std::uint32_t>(state.ex_c_addr_start);
  ex_req.loop_id = ex_id;
  ex_req.skip = state.is_resadd;
  ex_req_bits_ = ex_req;
  ex_req_val_ = bit(ex.configured == 1 && ex.ex_started == 0 &&
                    ex.lda_started == 1 && ex.ldb_started == 1 && ex.ldd_started == 1);

  LoopMatmulStCReq st_req{};
  st_req.max_k = state.is_resadd == 1 ? 1 : st.max_k;
  st_req.max_j = st.max_j;
  st_req.max_i = st.max_i;
  st_req.pad_j = st.pad_j;
  st_req.pad_i = st.pad_i;
  st_req.dram_addr = st.c_dram_addr;
  st_req.dram_stride = st.c_dram_stride;
  st_req.full_c = st.full_c;
  st_req.act = st.act;
  st_req.addr_start = state.is_resadd == 1 ? st.resadd_addr_start :
      static_cast<std::uint32_t>(state.st_c_addr_start);
  st_req.loop_id = st_id;
  st_req.is_resadd = state.is_resadd;
  st_c_req_bits_ = st_req;
  st_c_req_val_ = bit(st.configured == 1 && st.st_started == 0 &&
                     ((st.ex_started == 1 && st.spad_only == 0) || state.is_resadd == 1));

  LoopMatmulStCSpadReq spad_req{};
  spad_req.max_k = state.is_resadd == 1 ? 1 : st.max_k;
  spad_req.max_j = st.max_j;
  spad_req.max_i = st.max_i;
  spad_req.pad_j = st.pad_j;
  spad_req.pad_i = st.pad_i;
  spad_req.src_addr = static_cast<std::uint32_t>(state.st_c_addr_start);
  spad_req.dst_addr = st.c_spad_addr;
  spad_req.full_c = st.full_c;
  spad_req.act = st.act;
  spad_req.loop_id = st_id;
  spad_req.is_resadd = state.is_resadd;
  st_c_spad_req_bits_ = spad_req;
  st_c_spad_req_val_ = bit(st.configured == 1 && st.st_started == 0 &&
                          st.ex_started == 1 && st.spad_only == 1);

}

void LoopMatmul::updateCapacity() {
  const auto state = *state_Q_;
  st_utilization_at_limit_ = bit(state.st_outstanding >= kDefaultConfig.rs_store_entries);
  ex_utilization_at_limit_ = bit(state.ex_outstanding >= kDefaultConfig.rs_execute_entries);
}

void LoopMatmul::updateCommandFires() {
  ld_a_cmd_fire_ = bit(ld_a_cmd_val_ == 1 && ld_a_cmd_rdy_ == 1);
  ld_b_cmd_fire_ = bit(ld_b_cmd_val_ == 1 && ld_b_cmd_rdy_ == 1);
  ld_d_cmd_fire_ = bit(ld_d_cmd_val_ == 1 && ld_d_cmd_rdy_ == 1);
  ex_cmd_fire_ = bit(ex_cmd_val_ == 1 && ex_cmd_rdy_ == 1);
  st_c_cmd_fire_ = bit(st_c_cmd_val_ == 1 && st_c_cmd_rdy_ == 1);
  st_c_spad_cmd_fire_ = bit(st_c_spad_cmd_val_ == 1 && st_c_spad_cmd_rdy_ == 1);
}

void LoopMatmul::updateProgress() {
  const auto state = *state_Q_;
  lda_complete_to_ex_ = bit(ld_a_idle_ == 1 || ld_a_loop_id_ != ex_loop_id_);
  ldb_complete_to_ex_ = bit(ld_b_idle_ == 1 || ld_b_loop_id_ != ex_loop_id_);
  ldd_complete_to_ex_ = bit(ld_d_idle_ == 1 || ld_d_loop_id_ != ex_loop_id_);
  ex_complete_to_st_c_ = state.is_resadd == 1
      ? bit((ld_a_idle_ == 1 || ld_a_loop_id_ != st_c_loop_id_) &&
            (ld_b_idle_ == 1 || ld_b_loop_id_ != st_c_loop_id_))
      : bit(ex_idle_ == 1 || ex_loop_id_ != st_c_loop_id_);
  ex_complete_to_st_c_spad_ = bit(ex_idle_ == 1 ||
                                  ex_loop_id_ != st_c_spad_loop_id_);
}

void LoopMatmul::updateStatus() {
  const auto state = *state_Q_;
  const bool configured = state.loops[0].configured == 1 || state.loops[1].configured == 1;
  busy = bit(head_val_ == 1 || configured);
  head_loop_id = state.head_id;
  loop0 = state.loops[0];
  loop1 = state.loops[1];
  is_resadd_ = state.is_resadd;
  const auto head = static_cast<std::uint8_t>(state.head_id);
  const bool finished = state.loops[head].running == 1 &&
                        allCompleted(state.loops[head]);
  completed0 = bit(finished && head == 0);
  completed1 = bit(finished && head == 1);
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
  auto next = current;
  bool changed = false;
  const auto head_id = static_cast<std::uint8_t>(current.head_id);
  const auto tail_id = static_cast<std::uint8_t>(head_id ^ 1u);

  if (head_val_ == 1 && head_rdy_ == 1 &&
      isLoopCommand(commandFunct(*head_bits_))) {
    const auto head = *head_bits_;
    const auto funct = commandFunct(head);
    const auto selected_id = current.loops[head_id].configured == 1 ? tail_id : head_id;
    auto& slot = next.loops[selected_id];
    const auto rs1 = static_cast<std::uint64_t>(head.cmd.rs1);
    const auto rs2 = static_cast<std::uint64_t>(head.cmd.rs2);
    changed = true;
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
  }

  const auto requesting = [&](bit started) {
    return static_cast<std::uint8_t>(started == 1 ? tail_id : head_id);
  };
  if (ld_a_req_val_ == 1 && ld_a_req_rdy_ == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].lda_started)];
    slot.running = 1;
    slot.lda_started = 1;
    changed = true;
    trace("loop_matmul: launch LdA");
  }
  if (ld_b_req_val_ == 1 && ld_b_req_rdy_ == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].ldb_started)];
    slot.running = 1;
    slot.ldb_started = 1;
    changed = true;
    trace("loop_matmul: launch LdB");
  }
  if (ld_d_req_val_ == 1 && ld_d_req_rdy_ == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].ldd_started)];
    slot.running = 1;
    slot.ldd_started = 1;
    if (slot.c_dram_addr != 0)
      next.ld_d_addr_start = (static_cast<std::uint32_t>(current.ld_d_addr_start) +
                              kAccRows / 2) % kAccRows;
    changed = true;
    trace("loop_matmul: launch LdD");
  }
  if (ex_req_val_ == 1 && ex_req_rdy_ == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].ex_started)];
    slot.running = 1;
    slot.ex_started = 1;
    if ((slot.spad_only == 1 && slot.inc_acc_addr == 1) ||
        (slot.spad_only == 0 && slot.c_dram_addr != 0))
      next.ex_c_addr_start = (static_cast<std::uint32_t>(current.ex_c_addr_start) +
                              kAccRows / 2) % kAccRows;
    changed = true;
    trace("loop_matmul: launch Ex");
  }
  if ((st_c_req_val_ == 1 && st_c_req_rdy_ == 1) ||
      (st_c_spad_req_val_ == 1 && st_c_spad_req_rdy_ == 1)) {
    auto& slot = next.loops[requesting(current.loops[head_id].st_started)];
    slot.running = 1;
    slot.st_started = 1;
    if ((slot.spad_only == 1 && slot.inc_acc_addr == 1) ||
        (slot.spad_only == 0 && slot.c_dram_addr != 0))
      next.st_c_addr_start = (static_cast<std::uint32_t>(current.st_c_addr_start) +
                              kAccRows / 2) % kAccRows;
    changed = true;
    trace("loop_matmul: launch StC");
  }

  const auto complete = [&](bit idle, u8 loop_id, bit LoopMatmulSlot::*started,
                            bit LoopMatmulSlot::*completed) {
    const auto id = static_cast<std::uint8_t>(loop_id);
    if (idle == 1 && id < next.loops.size() &&
        current.loops[id].running == 1 && current.loops[id].*started == 1 &&
        current.loops[id].*completed == 0) {
      next.loops[id].*completed = 1;
      changed = true;
    }
  };
  complete(*ld_a_idle_, *ld_a_loop_id_, &LoopMatmulSlot::lda_started,
           &LoopMatmulSlot::lda_completed);
  complete(*ld_b_idle_, *ld_b_loop_id_, &LoopMatmulSlot::ldb_started,
           &LoopMatmulSlot::ldb_completed);
  complete(*ld_d_idle_, *ld_d_loop_id_, &LoopMatmulSlot::ldd_started,
           &LoopMatmulSlot::ldd_completed);
  complete(*ex_idle_, *ex_loop_id_, &LoopMatmulSlot::ex_started,
           &LoopMatmulSlot::ex_completed);
  if (st_c_idle_ == 1 && st_c_loop_id_ < next.loops.size() &&
      current.loops[static_cast<std::uint8_t>(*st_c_loop_id_)].spad_only == 0)
    complete(*st_c_idle_, *st_c_loop_id_, &LoopMatmulSlot::st_started,
             &LoopMatmulSlot::st_completed);
  if (st_c_spad_idle_ == 1 && st_c_spad_loop_id_ < next.loops.size() &&
      current.loops[static_cast<std::uint8_t>(*st_c_spad_loop_id_)].spad_only == 1)
    complete(*st_c_spad_idle_, *st_c_spad_loop_id_, &LoopMatmulSlot::st_started,
             &LoopMatmulSlot::st_completed);

  const auto st_count = static_cast<std::uint16_t>(current.st_outstanding);
  const auto ex_count = static_cast<std::uint16_t>(current.ex_outstanding);
  const auto st_done = static_cast<std::uint8_t>(*st_completed);
  const auto ex_done = static_cast<std::uint8_t>(*ex_completed);
  const unsigned st_issued = st_c_cmd_fire_ == 1 || st_c_spad_cmd_fire_ == 1;
  const unsigned ex_issued = ex_cmd_fire_ == 1;
  assert(st_done <= st_count && ex_done <= ex_count);
  const auto next_st_count = st_count + st_issued - st_done;
  const auto next_ex_count = ex_count + ex_issued - ex_done;
  assert(next_st_count <= kDefaultConfig.rs_store_entries);
  assert(next_ex_count <= kDefaultConfig.rs_execute_entries);
  if (next_st_count != st_count || next_ex_count != ex_count) {
    next.st_outstanding = static_cast<std::uint16_t>(next_st_count);
    next.ex_outstanding = static_cast<std::uint16_t>(next_ex_count);
    changed = true;
    trace("loop_matmul: utilization st=%u ex=%u", next_st_count, next_ex_count);
  }

  if (completed0 == 1 || completed1 == 1) {
    clearLoopProgress(next.loops[head_id]);
    next.head_id = tail_id;
    changed = true;
    trace("loop_matmul: complete loop=%u", static_cast<unsigned>(head_id));
  }

  if (changed) state_D_ = next;
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
  completed0.reset(0);
  completed1.reset(0);
  is_resadd_.reset(0);
  ld_a_req_val_.reset(0);
  ld_b_req_val_.reset(0);
  ld_d_req_val_.reset(0);
  ex_req_val_.reset(0);
  st_c_req_val_.reset(0);
  st_c_spad_req_val_.reset(0);
  ld_a_req_bits_.reset(LoopMatmulLdAReq{});
  ld_b_req_bits_.reset(LoopMatmulLdBReq{});
  ld_d_req_bits_.reset(LoopMatmulLdDReq{});
  ex_req_bits_.reset(LoopMatmulExReq{});
  st_c_req_bits_.reset(LoopMatmulStCReq{});
  st_c_spad_req_bits_.reset(LoopMatmulStCSpadReq{});
  st_utilization_at_limit_.reset(0);
  ex_utilization_at_limit_.reset(0);
  ld_a_cmd_fire_.reset(0);
  ld_b_cmd_fire_.reset(0);
  ld_d_cmd_fire_.reset(0);
  ex_cmd_fire_.reset(0);
  st_c_cmd_fire_.reset(0);
  st_c_spad_cmd_fire_.reset(0);
  lda_complete_to_ex_.reset(0);
  ldb_complete_to_ex_.reset(0);
  ldd_complete_to_ex_.reset(0);
  ex_complete_to_st_c_.reset(0);
  ex_complete_to_st_c_spad_.reset(0);
}

} // namespace smesh
