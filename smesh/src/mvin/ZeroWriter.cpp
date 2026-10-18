// **********************************************************************
// smesh/src/mvin/ZeroWriter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 18 2026

#include "ZeroWriter.hpp"
#include "SmeshCommand.hpp"

#include <algorithm>

namespace smesh {

ZeroWriter::ZeroWriter(std::string /*name*/, IMPL_CTOR) {
  state_Q_ <= state_D_;
  UPDATE(updateResponse).reads(state_Q_).writes(resp_val, resp_bits);
  UPDATE(updateReady).reads(state_Q_, resp_val, resp_bits, resp_rdy).writes(req_rdy);
  UPDATE(updateState).reads(state_Q_, req_val, req_bits, req_rdy,
                            resp_val, resp_bits, resp_rdy).writes(state_D_);
}

// Generate zeros and a lane mask for the next local row of the saved request.
void ZeroWriter::updateResponse() {
  const auto state = *state_Q_;
  DmaReadResp row{};
  if (state.valid == 1) {
    const auto& req = state.request;
    const auto column = static_cast<unsigned>(state.column);
    const auto cols = static_cast<unsigned>(req.cols);
    const auto count = std::min<unsigned>(kDim, cols - column);
    row.laddr = req.laddr + (column / kDim) * static_cast<unsigned>(req.block_stride);
    row.mask = u32((std::uint64_t{1} << count) - 1);
    row.len = req.cols;
    row.has_acc_bitwidth = req.has_acc_bitwidth;
    row.scale = kMvinScaleIdentityBits;
    row.pixel_repeats = req.pixel_repeats;
    row.cmd_id = req.cmd_id;
    row.last = bit(column + kDim >= cols);
    // Match the load tracker's byte units, including narrow accumulator loads.
    // Original counts all accumulator zero fills at accumulator width instead;
    // copying that expression would over-count our shrink-mode tracker entries.
    row.bytes_read = u16(cols * (req.has_acc_bitwidth == 1 ? sizeof(Acc) : sizeof(Elem)));
  }
  resp_val = state.valid;
  resp_bits = row;
}

// The last accepted row frees this worker for a new request in the same cycle.
void ZeroWriter::updateReady() {
  req_rdy = bit(state_Q_->valid == 0 ||
                (resp_val == 1 && resp_rdy == 1 && resp_bits->last == 1));
}

// Advance only on an accepted row; a newly accepted request replaces finished work.
void ZeroWriter::updateState() {
  auto next = *state_Q_;
  const bool pop = resp_val == 1 && resp_rdy == 1;
  const bool push = req_val == 1 && req_rdy == 1;
  if (!pop && !push) return;
  if (pop) {
    trace("zero_writer: row addr=0x%x mask=0x%x cmd_id=%u last=%u\n",
          unsigned(resp_bits->laddr.raw), unsigned(resp_bits->mask),
          unsigned(resp_bits->cmd_id), unsigned(resp_bits->last));
    if (resp_bits->last == 1) next = State{};
    else next.column = u16(static_cast<unsigned>(next.column) + kDim);
  }
  if (push) {
    const auto req = *req_bits;
    constexpr auto max_cols = std::max(kDefaultConfig.dma_max_bytes / sizeof(Elem), kDim);
    assert_always(req.all_zeros == 1 && req.cols > 0 && req.cols <= max_cols,
                  "ZeroWriter requires a nonempty zero-fill request within its column capacity");
    assert_always(req.cols <= kDim || req.block_stride > 0,
                  "ZeroWriter needs a block stride for multi-row requests");
    next = State{};
    next.valid = 1;
    next.request = req;
    trace("zero_writer: request addr=0x%x cols=%u cmd_id=%u\n",
          unsigned(req.laddr.raw), unsigned(req.cols), unsigned(req.cmd_id));
  }
  state_D_ = next;
}

void ZeroWriter::reset() {
  state_D_.reset(State{});
  req_rdy.reset(0);
  resp_val.reset(0);
  resp_bits.reset(DmaReadResp{});
}

} // namespace smesh
