// **********************************************************************
// smesh/src/loop_matmul/LoopMatmulState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 13 2026

#include "LoopMatmulState.hpp"

#include "SmeshCommand.hpp"

namespace smesh {
namespace {

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

LoopMatmulState::LoopMatmulState(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateView).reads(state_Q_)
      .writes(state, loop0, loop1, head_loop_id, completed0, completed1);
  UPDATE(updateNextState)
      .reads(state_Q_, head_val, head_bits, head_rdy)
      .reads(ld_a_req_val, ld_a_req_rdy, ld_b_req_val, ld_b_req_rdy,
             ld_d_req_val, ld_d_req_rdy, ex_req_val, ex_req_rdy)
      .reads(st_c_req_val, st_c_req_rdy, st_c_spad_req_val, st_c_spad_req_rdy)
      .reads(ld_a_idle, ld_a_loop_id, ld_b_idle, ld_b_loop_id,
             ld_d_idle, ld_d_loop_id, ex_idle, ex_loop_id)
      .reads(st_c_idle, st_c_loop_id, st_c_spad_idle, st_c_spad_loop_id)
      .reads(completed0, completed1)
      .writes(state_D_);
}

// Publish current slot contents and signal when the head command stream has finished.
void LoopMatmulState::updateView() {
  const auto current = *state_Q_;
  state = current;
  loop0 = current.loops[0];
  loop1 = current.loops[1];
  head_loop_id = current.head_id;
  const auto head = static_cast<std::uint8_t>(current.head_id);
  const bool finished = current.loops[head].running == 1 && allCompleted(current.loops[head]);
  completed0 = bit(finished && head == 0);
  completed1 = bit(finished && head == 1);
}

// Save accepted setup, record worker starts/completion, and retire a finished head slot.
void LoopMatmulState::updateNextState() {
  const auto current = *state_Q_;
  auto next = current;
  bool changed = false;
  const auto head_id = static_cast<std::uint8_t>(current.head_id);
  const auto tail_id = static_cast<std::uint8_t>(head_id ^ 1u);

  if (head_val == 1 && head_rdy == 1) {
    const auto head = *head_bits;
    const auto funct = static_cast<SmeshFunct>(static_cast<std::uint32_t>(head.cmd.funct));
    if (isLoopCommand(funct)) {
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
          slot.spad_only = 0;
          slot.a_dram_addr = rs1;
          slot.b_dram_addr = rs2;
          break;
        case SmeshFunct::LoopWsAddrsDc:
          slot.spad_only = 0;
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
          slot.full_c = bit((rs1 >> 1) & 1u);
          slot.low_d = bit((rs1 >> 2) & 1u);
          slot.act = (rs1 >> 8) & 0x7u;
          slot.b_ex_spad_id = (rs1 >> 16) & 0x3u;
          slot.a_ex_spad_id = (rs1 >> 18) & 0x3u;
          slot.a_transpose = bit(rs2 & 1u);
          slot.b_transpose = bit((rs2 >> 1) & 1u);
          next.is_resadd = bit((rs2 >> 2) & 1u);
          slot.lda_started = bit((rs2 >> 3) & 1u);
          slot.ldb_started = bit((rs2 >> 4) & 1u);
          slot.ldd_started = bit((rs2 >> 5) & 1u);
          slot.ex_started = bit((rs2 >> 6) & 1u);
          slot.st_started = bit((rs2 >> 7) & 1u);
          slot.lda_completed = slot.lda_started;
          slot.ldb_completed = slot.ldb_started;
          slot.ldd_completed = slot.ldd_started;
          slot.ex_completed = slot.ex_started;
          slot.st_completed = slot.st_started;
          slot.inc_acc_addr = bit((rs2 >> 8) & 1u);
          slot.spad_only = bit((rs2 >> 9) & 1u);
          slot.c_spad_addr = rs2 >> 32;
          slot.configured = 1;
          break;
        default:
          break;
      }
      trace("loop_matmul: slot=%u funct=%u configured=%u",
            static_cast<unsigned>(selected_id), static_cast<unsigned>(funct),
            static_cast<unsigned>(slot.configured == 1));
    }
  }

  // Each worker starts the head job first, then the other slot's job.
  const auto requesting = [&](bit started) {
    return static_cast<std::uint8_t>(started == 1 ? tail_id : head_id);
  };
  if (ld_a_req_val == 1 && ld_a_req_rdy == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].lda_started)];
    slot.running = 1;
    slot.lda_started = 1;
    changed = true;
    trace("loop_matmul: launch LdA");
  }
  if (ld_b_req_val == 1 && ld_b_req_rdy == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].ldb_started)];
    slot.running = 1;
    slot.ldb_started = 1;
    changed = true;
    trace("loop_matmul: launch LdB");
  }
  if (ld_d_req_val == 1 && ld_d_req_rdy == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].ldd_started)];
    slot.running = 1;
    slot.ldd_started = 1;
    if (slot.c_dram_addr != 0)
      next.ld_d_addr_start = (static_cast<std::uint32_t>(current.ld_d_addr_start) + kAccRows / 2) % kAccRows;
    changed = true;
    trace("loop_matmul: launch LdD");
  }
  if (ex_req_val == 1 && ex_req_rdy == 1) {
    auto& slot = next.loops[requesting(current.loops[head_id].ex_started)];
    slot.running = 1;
    slot.ex_started = 1;
    if ((slot.spad_only == 1 && slot.inc_acc_addr == 1) ||
        (slot.spad_only == 0 && slot.c_dram_addr != 0))
      next.ex_c_addr_start = (static_cast<std::uint32_t>(current.ex_c_addr_start) + kAccRows / 2) % kAccRows;
    changed = true;
    trace("loop_matmul: launch Ex");
  }
  if ((st_c_req_val == 1 && st_c_req_rdy == 1) ||
      (st_c_spad_req_val == 1 && st_c_spad_req_rdy == 1)) {
    auto& slot = next.loops[requesting(current.loops[head_id].st_started)];
    slot.running = 1;
    slot.st_started = 1;
    if ((slot.spad_only == 1 && slot.inc_acc_addr == 1) ||
        (slot.spad_only == 0 && slot.c_dram_addr != 0))
      next.st_c_addr_start = (static_cast<std::uint32_t>(current.st_c_addr_start) + kAccRows / 2) % kAccRows;
    changed = true;
    trace("loop_matmul: launch StC");
  }

  // Idle marks that worker's primitive command stream complete for its saved loop ID.
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
  complete(*ld_a_idle, *ld_a_loop_id, &LoopMatmulSlot::lda_started, &LoopMatmulSlot::lda_completed);
  complete(*ld_b_idle, *ld_b_loop_id, &LoopMatmulSlot::ldb_started, &LoopMatmulSlot::ldb_completed);
  complete(*ld_d_idle, *ld_d_loop_id, &LoopMatmulSlot::ldd_started, &LoopMatmulSlot::ldd_completed);
  complete(*ex_idle, *ex_loop_id, &LoopMatmulSlot::ex_started, &LoopMatmulSlot::ex_completed);
  if (st_c_idle == 1 && st_c_loop_id < next.loops.size() &&
      current.loops[static_cast<std::uint8_t>(*st_c_loop_id)].spad_only == 0)
    complete(*st_c_idle, *st_c_loop_id, &LoopMatmulSlot::st_started, &LoopMatmulSlot::st_completed);
  if (st_c_spad_idle == 1 && st_c_spad_loop_id < next.loops.size() &&
      current.loops[static_cast<std::uint8_t>(*st_c_spad_loop_id)].spad_only == 1)
    complete(*st_c_spad_idle, *st_c_spad_loop_id, &LoopMatmulSlot::st_started, &LoopMatmulSlot::st_completed);

  // Release the finished head slot and select the other slot at the next clock edge.
  if (completed0 == 1 || completed1 == 1) {
    clearLoopProgress(next.loops[head_id]);
    next.head_id = tail_id;
    changed = true;
    trace("loop_matmul: complete loop=%u", static_cast<unsigned>(head_id));
  }
  if (changed) state_D_ = next;
}

void LoopMatmulState::reset() {
  LoopMatmulStateData initial{};
  const auto half_spad = static_cast<std::uint32_t>(kSpRows / 2);
  const auto half_acc = static_cast<std::uint32_t>(kAccRows / 2);
  for (std::size_t id = 0; id < initial.loops.size(); ++id) {
    initial.loops[id].a_addr_start = id * half_spad;
    initial.loops[id].b_addr_end = (id + 1) * half_spad;
    initial.loops[id].resadd_addr_start = id * half_acc;
  }
  state_D_.reset(initial);
  state.reset(initial);
  loop0.reset(initial.loops[0]);
  loop1.reset(initial.loops[1]);
  head_loop_id.reset(0);
  completed0.reset(0);
  completed1.reset(0);
}

} // namespace smesh
