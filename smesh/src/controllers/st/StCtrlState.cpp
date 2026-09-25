// **********************************************************************
// smesh/src/controllers/st/StCtrlState.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 22 2026

#include "StCtrlState.hpp"

namespace smesh {

namespace {

std::uint32_t wrappingAdd(std::uint32_t value, std::uint32_t modulus) {
  return modulus == 0 || value + 1 >= modulus ? 0 : value + 1;
}

} // namespace

TraceKey(st_ctrl_state_);

StCtrlState::StCtrlState(std::string /*name*/, IMPL_CTOR) {
  regs_Q_ <= regs_D_;

  UPDATE(updateConfigView)
      .reads(regs_Q_)
      .writes(control_state, stride, activation, acc_scale, igelu_qb, igelu_qc,
              iexp_qln2, iexp_qln2_inv)
      .writes(norm_stats_id, pool_stride, pool_size, pool_out_dim, pool_porows,
              pool_pocols, pool_orows, pool_ocols)
      .writes(pool_upad, pool_lpad, row_counter, block_counter, porow_counter,
              pocol_counter, wrow_counter, wcol_counter);

  UPDATE(updateRequest)
      .reads(regs_Q_, head_val, head_bits, do_store, tracker_alloc_rdy,
             tracker_alloc_cmd_id, pooling_is_enabled, mvout_1d_enabled)
      .reads(rows, blocks, mvout_1d_rows, pool_total_rows)
      .writes(tracker_alloc_val, tracker_alloc_response_count,
              tracker_alloc_rs_tag, dma_req_val, dma_req_cmd_id);

  UPDATE(updateTransition)
      .reads(regs_Q_, head_val, do_config, do_config_norm, do_store,
             tracker_alloc_val, tracker_alloc_rdy, tracker_alloc_cmd_id)
      .reads(dma_req_val, dma_req_rdy, pooling_is_enabled, mvout_1d_enabled,
             rows, blocks, localaddr, mvout_1d_rows)
      .reads(dma_req_cmd_id, config_stride, config_activation, config_acc_scale,
             config_pool_stride, config_pool_size, config_pool_out_dim,
             config_porows)
      .reads(config_pocols, config_orows, config_ocols, config_upad, config_lpad,
             config_stats_id, config_activation_msb, config_set_stats_id_only)
      .reads(config_iexp_q_const_type, config_iexp_q_const, config_igelu_qb,
             config_igelu_qc)
      .writes(head_rdy, regs_D_);
}

void StCtrlState::updateConfigView() {
  const auto q  = *regs_Q_;
  control_state = q.state;
  // persistent configuration registers
  stride        = q.stride;
  activation    = q.activation;
  acc_scale     = q.acc_scale;
  igelu_qb      = q.igelu_qb;
  igelu_qc      = q.igelu_qc;
  iexp_qln2     = q.iexp_qln2;
  iexp_qln2_inv = q.iexp_qln2_inv;
  norm_stats_id = q.norm_stats_id;
  pool_stride   = q.pool_stride;
  pool_size     = q.pool_size;
  pool_out_dim  = q.pool_out_dim;
  pool_porows   = q.pool_porows;
  pool_pocols   = q.pool_pocols;
  pool_orows    = q.pool_orows;
  pool_ocols    = q.pool_ocols;
  pool_upad     = q.pool_upad;
  pool_lpad     = q.pool_lpad;
  // counters
  row_counter   = q.row_counter;
  block_counter = q.block_counter;
  porow_counter = q.porow_counter;
  pocol_counter = q.pocol_counter;
  wrow_counter  = q.wrow_counter;
  wcol_counter  = q.wcol_counter;
}

// Compute current cycle's o/p from registered FSM state & cmd head
// - whether to allocate a tracker ID entry for current cmd
// - how many responses to expect for current cmd (for pooling, mvout_1d, or normal store)
// - whether a DMA request is valid for current cycle (for waiting, sending, or pooling)
// - which tracker ID to use for current DMA request (if allocated this cycle, use new ID)
void StCtrlState::updateRequest() {
  const auto q             = *regs_Q_;
  const auto state         = static_cast<StCtrlFsmState>(q.state);
  const bool waiting       = state == StCtrlFsmState::WaitingForCommand;
  const bool store_head    = head_val == 1 && do_store == 1; // STORE_CMD is here
  const bool allocate      = waiting && store_head;
  const bool allocated_now = allocate && tracker_alloc_rdy == 1;

  tracker_alloc_val            = bit(allocate);
  tracker_alloc_rs_tag         = store_head ? head_bits->rs_tag : 0;
  tracker_alloc_response_count = pooling_is_enabled == 1
      ? static_cast<std::uint32_t>(*pool_total_rows)
      : (mvout_1d_enabled == 1
             ? static_cast<std::uint32_t>(*mvout_1d_rows)
             : static_cast<std::uint32_t>(*rows) * static_cast<std::uint32_t>(*blocks));

  dma_req_val = bit(allocated_now ||
                    state == StCtrlFsmState::WaitingForDmaReqReady ||
                    (state == StCtrlFsmState::SendingRows &&
                     (q.block_counter != 0 || q.row_counter != 0)) ||
                    (state == StCtrlFsmState::Pooling &&
                     (q.wcol_counter != 0 || q.wrow_counter != 0 ||
                      q.pocol_counter != 0 || q.porow_counter != 0)));
  // The first request sees the newly selected tracker ID in allocation's cycle.
  dma_req_cmd_id = allocated_now ? static_cast<std::uint16_t>(*tracker_alloc_cmd_id)
                                 : q.cmd_id;
}

// handles next state
// - updates configuration and counters
// - changes FSM state when req fires or cmd finishes
// - asserts heady_rdy when queue cmd can be popped
void StCtrlState::updateTransition() {
  const auto q = *regs_Q_; // current state
  auto next = q;           // next state to be computed
  const auto state      = static_cast<StCtrlFsmState>(q.state);
  const bool req_fire   = dma_req_val == 1 && dma_req_rdy == 1;
  const bool pooling    = pooling_is_enabled == 1;
  const bool moveout_1d = mvout_1d_enabled == 1;
  const auto nblocks    = static_cast<std::uint32_t>(*blocks);
  const auto nrows      = static_cast<std::uint32_t>(*rows);
  const auto n1drows    = static_cast<std::uint32_t>(*mvout_1d_rows);
  head_rdy = 0;
  // State transition logic
  if (state == StCtrlFsmState::WaitingForCommand && head_val == 1) {
    // Store CONFIG commands need RS issue completion, not a DMA tracker completion.
    if (do_config == 1) { // CONFIG_STORE branch, copy decoded settings into next
      next.stride         = *config_stride;
      next.activation     = *config_activation;
      if (*config_acc_scale != 0xffffffffu) { // all 1's means keep current scale
        next.acc_scale    = *config_acc_scale;
      }
      next.pool_size      = *config_pool_size;
      next.pool_stride    = *config_pool_stride;
      if (config_pool_stride != 0) {
        next.pool_out_dim = *config_pool_out_dim;
        next.pool_porows  = *config_porows;
        next.pool_pocols  = *config_pocols;
        next.pool_orows   = *config_orows;
        next.pool_ocols   = *config_ocols;
        next.pool_upad    = *config_upad;
        next.pool_lpad    = *config_lpad;
      } else if (config_pool_size != 0) {
        next.pool_orows   = *config_orows;
        next.pool_ocols   = *config_ocols;
        next.pool_out_dim = *config_pool_out_dim;
      }
      head_rdy = 1;
      trace(st_ctrl_state_, "config_store stride=%u pool=%u/%u\n",
            static_cast<unsigned>(next.stride),
            static_cast<unsigned>(next.pool_stride),
            static_cast<unsigned>(next.pool_size));
    } else if (do_config_norm == 1) { // CONFIG_NORM branch
      // update just stats ID or update norm settings too
      if (config_set_stats_id_only == 0) {
        next.igelu_qb = *config_igelu_qb;
        next.igelu_qc = *config_igelu_qc;
        if (config_iexp_q_const_type == 0) {
          next.iexp_qln2 = *config_iexp_q_const;
        } else {
          next.iexp_qln2_inv = *config_iexp_q_const;
        }
        next.activation = static_cast<std::uint8_t>(
            ((config_activation_msb == 1 ? 1u : 0u) << 2) | (q.activation & 0x3u));
      }
      next.norm_stats_id = *config_stats_id; // which norm stat context to use
      head_rdy = 1;
      trace(st_ctrl_state_, "config_norm stats=%u msb=%u only=%u act=%u\n",
            static_cast<unsigned>(next.norm_stats_id),
            static_cast<unsigned>(config_activation_msb == 1),
            static_cast<unsigned>(config_set_stats_id_only == 1),
            static_cast<unsigned>(next.activation));
    } else if (do_store == 1 && tracker_alloc_rdy == 1) { // STORE branch (also needs free tracker entry)
      next.cmd_id = *tracker_alloc_cmd_id; // record allocated tracker ID so later DMA resp can be matched to this cmd
      // first DMA req may be accepted in same cyc as tracker alloc, if so it enters pooling or sending rows right away
      // otherwise it waits
      next.state  = static_cast<std::uint8_t>(
          req_fire ? (pooling ? StCtrlFsmState::Pooling : StCtrlFsmState::SendingRows)
                   : StCtrlFsmState::WaitingForDmaReqReady);
      trace(st_ctrl_state_, "start id=%u first_fire=%u state=%u\n",
            static_cast<unsigned>(next.cmd_id),
            static_cast<unsigned>(req_fire),
            static_cast<unsigned>(next.state));
    }
  } else if (state == StCtrlFsmState::WaitingForDmaReqReady) {
    if (req_fire) {
      next.state = static_cast<std::uint8_t>(
          pooling ? StCtrlFsmState::Pooling : StCtrlFsmState::SendingRows);
      trace(st_ctrl_state_, "first_fire id=%u state=%u\n",
            static_cast<unsigned>(q.cmd_id), static_cast<unsigned>(next.state));
    }
  } else if (state == StCtrlFsmState::SendingRows) {
    const bool last_block     = nblocks != 0 && q.block_counter == nblocks - 1;
    const auto effective_rows = moveout_1d ? n1drows : nrows;
    const bool last_row       = effective_rows != 0 && q.row_counter == effective_rows - 1;
    const bool only_one_req   = q.block_counter == 0 && q.row_counter == 0;
    if ((last_block && last_row && req_fire) || only_one_req) { // last row accepted?
      next.state = static_cast<std::uint8_t>(StCtrlFsmState::WaitingForCommand);
      head_rdy = 1;
      trace(st_ctrl_state_, "finish_rows id=%u\n", static_cast<unsigned>(q.cmd_id));
    }
  } else if (state == StCtrlFsmState::Pooling) {
    const auto psize = static_cast<std::uint32_t>(q.pool_size);
    const auto prows = static_cast<std::uint32_t>(q.pool_porows);
    const auto pcols = static_cast<std::uint32_t>(q.pool_pocols);
    const bool all_zero = q.porow_counter == 0 && q.pocol_counter == 0 &&
                          q.wrow_counter == 0 && q.wcol_counter == 0;
    const bool final_position = psize != 0 && prows != 0 && pcols != 0 &&
        q.porow_counter == prows - 1 && q.pocol_counter == pcols - 1 &&
        q.wrow_counter == psize - 1 && q.wcol_counter == psize - 1;
    if (all_zero || (final_position && req_fire)) {
      next.state = static_cast<std::uint8_t>(StCtrlFsmState::WaitingForCommand);
      head_rdy = 1;
      trace(st_ctrl_state_, "finish_pool id=%u\n", static_cast<unsigned>(q.cmd_id));
    }
  }

  if (req_fire) {
    assert_always(!(nblocks > 1 && localaddr->read_full_acc_row()),
                  "Full accumulator row cannot span multiple blocks");
    assert_always(!(nblocks > 1 && (pooling || moveout_1d)),
                  "Pooling and 1-D moveout require one block");
    if (!pooling) {
      if (moveout_1d) {
        const auto pcols = static_cast<std::uint32_t>(q.pool_ocols);
        next.pocol_counter = wrappingAdd(q.pocol_counter, pcols);
        if (pcols != 0 && q.pocol_counter == pcols - 1) {
          next.porow_counter = wrappingAdd(q.porow_counter, q.pool_orows);
        }
      }
      next.block_counter = wrappingAdd(q.block_counter, nblocks);
      if (moveout_1d) {
        next.row_counter = wrappingAdd(q.row_counter, n1drows);
      } else if (nblocks != 0 && q.block_counter == nblocks - 1) {
        next.row_counter = wrappingAdd(q.row_counter, nrows);
      }
    } else {
      const auto psize = static_cast<std::uint32_t>(q.pool_size);
      const auto pcols = static_cast<std::uint32_t>(q.pool_pocols);
      next.wcol_counter = wrappingAdd(q.wcol_counter, psize);
      if (psize != 0 && q.wcol_counter == psize - 1) {
        next.wrow_counter = wrappingAdd(q.wrow_counter, psize);
        if (q.wrow_counter == psize - 1) {
          next.pocol_counter = wrappingAdd(q.pocol_counter, pcols);
          if (pcols != 0 && q.pocol_counter == pcols - 1) {
            next.porow_counter = wrappingAdd(q.porow_counter, q.pool_porows);
          }
        }
      }
    }
    trace(st_ctrl_state_, "req_fire id=%u row=%u block=%u pool=%u,%u,%u,%u\n",
          static_cast<unsigned>(*dma_req_cmd_id), static_cast<unsigned>(q.row_counter),
          static_cast<unsigned>(q.block_counter),
          static_cast<unsigned>(q.porow_counter), static_cast<unsigned>(q.pocol_counter),
          static_cast<unsigned>(q.wrow_counter), static_cast<unsigned>(q.wcol_counter));
  }

  regs_D_ = next;
}

void StCtrlState::reset() {
  regs_D_.reset(StCtrlStateRegs{});
  control_state.reset(0);
  stride.reset(0);
  activation.reset(0);
  acc_scale.reset(0);
  igelu_qb.reset(0);
  igelu_qc.reset(0);
  iexp_qln2.reset(0);
  iexp_qln2_inv.reset(0);
  norm_stats_id.reset(0);
  pool_stride.reset(0);
  pool_size.reset(0);
  pool_out_dim.reset(0);
  pool_porows.reset(0);
  pool_pocols.reset(0);
  pool_orows.reset(0);
  pool_ocols.reset(0);
  pool_upad.reset(0);
  pool_lpad.reset(0);
  row_counter.reset(0);
  block_counter.reset(0);
  porow_counter.reset(0);
  pocol_counter.reset(0);
  wrow_counter.reset(0);
  wcol_counter.reset(0);
  tracker_alloc_val.reset(0);
  tracker_alloc_response_count.reset(0);
  tracker_alloc_rs_tag.reset(0);
  dma_req_val.reset(0);
  dma_req_cmd_id.reset(0);
  head_rdy.reset(0);
}

} // namespace smesh
