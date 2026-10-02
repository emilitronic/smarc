// **********************************************************************
// smesh/src/dma/DmaReader.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Not unreasonably minimal DMA reader implementation.
*/

#include "DmaReader.hpp"

#include <algorithm>

namespace smesh {

DmaReader::DmaReader(std::string /*name*/, IMPL_CTOR) {
  pending_Q_ <= pending_D_;
  state_Q_   <= state_D_;
  UPDATE(updateRespView).reads(pending_Q_).writes(resp_val, resp_bits);
  UPDATE(update).reads(req_in, mem_resp, pending_Q_, state_Q_, resp_rdy)
      .writes(mem_req, pending_D_, state_D_);
}

void DmaReader::updateRespView() {
  const auto pending = *pending_Q_;
  resp_val  = bit(pending.count != 0);
  resp_bits = pending.count != 0 ? pending.first : DmaReadResp{};
}

void DmaReader::update() {
  auto pending         = *pending_Q_;
  auto state           = *state_Q_;
  auto pending_count   = static_cast<unsigned>(static_cast<std::uint8_t>(pending.count));
  bool pending_changed = false;
  bool state_changed   = false;

  // remove completed row from pending buffer if consumer ready to accept it
  if (pending_count > 0 && resp_rdy == 1) {
    pending.first  = pending_count == 2 ? pending.second : DmaReadResp{};
    pending.second = DmaReadResp{};
    --pending_count;
    pending_changed = true;
  }

  // handle a mem resp for beat DmaReader req'd (for one mem req, not entire DmaReadReq)
  if (state.waiting == 1 && state.beat_bytes == 0 && !mem_resp.empty()) {
    const auto resp   = mem_resp.pop(); // pop mem_resp for active read req
    assert_always(static_cast<std::uint16_t>(resp.id) == static_cast<std::uint16_t>(state.active.cmd_id), "DmaReader response ID does not match active request");
    assert_always(static_cast<std::uint8_t>(resp.err) == 0, "DmaReader memory response reported an error");
    state.beat_data   = static_cast<std::uint64_t>(resp.rdata);
    state.beat_bytes  = state.requested_beat_bytes;
    state.beat_offset = 0;
    state.waiting     = 0;
    state_changed     = true;
    trace("dma_reader: response data=0x%llx cmd_id=%u\n",
          static_cast<unsigned long long>(resp.rdata), static_cast<unsigned>(resp.id));
  }

  const auto element_bytes = state.active.has_acc_bitwidth == 1 ? sizeof(Acc) : sizeof(Elem);
  const auto row_bytes     = kDim * element_bytes; // bytes that make up a complete row

  // assemble received data: move bytes from saved memory beat into current o/p row one byte at a time
  // stop when beat is consumed or two-entry o/p buffer is full (pending_count == 2)
  while (state.beat_offset < state.beat_bytes && pending_count < 2) {
    //select next byte from beat and place it in next available position in row_data
    state.row_data[state.row_fill++] = static_cast<std::uint8_t>(state.beat_data >> (8 * state.beat_offset++));
    ++state.bytes_received;
    state_changed = true;
    // when row is filled or all bytes for active req have been received, form a DmaReadResp and place it in pending buffer
    if (state.row_fill == row_bytes || state.bytes_received == state.total_bytes) {
      const auto cols      = state.row_fill / element_bytes; // how many elements are present
      const auto row_index = (state.bytes_received - state.row_fill) / row_bytes;
      DmaReadResp row{};
      row.data             = state.row_data;
      row.laddr            = state.active.laddr + row_index * static_cast<std::uint16_t>(state.active.block_stride);
      row.mask             = u32((std::uint64_t{1} << cols) - 1u);
      row.has_acc_bitwidth = state.active.has_acc_bitwidth;
      row.scale            = state.active.scale;
      row.repeats          = state.active.repeats;
      row.len              = u16(cols);
      row.bytes_read       = u16(state.row_fill);
      row.pixel_repeats    = state.active.pixel_repeats;
      row.cmd_id           = state.active.cmd_id;
      row.last             = 1;
      // append completed row to pending buffer
      if (pending_count == 0) pending.first = row;
      else pending.second = row;
      ++pending_count;
      pending_changed      = true;
      state.row_data       = {}; // clear partial-row buffer so subsequent bytes can form next row
      state.row_fill       = 0;
      trace("dma_reader: row addr=0x%x cols=%u bytes=%u cmd_id=%u\n",
            static_cast<unsigned>(row.laddr.raw), static_cast<unsigned>(cols),
            static_cast<unsigned>(row.bytes_read), static_cast<unsigned>(row.cmd_id));
    }
  }

  // detect that every byte in currently saved mem beat has been consumed by row-assembly loop
  // when beat_offset = beat_bytes, all req'd bytes from resp have been read
  if (state.beat_bytes != 0 && state.beat_offset == state.beat_bytes) {
    state.beat_bytes  = 0; // no saved beat left to process
    state.beat_offset = 0; // reset offset for next resp beat
    state_changed     = true;
  }

  // mark active read req to DmaReader as finished once all its bytes have been consumed
  if (state.active_valid == 1 && state.bytes_received == state.total_bytes && state.beat_bytes == 0 && state.waiting == 0) {
    state.active_valid = 0;
    state_changed      = true;
  }
  
  // Pop (in) req: if there's no active req, outgoing mem req FIFO has room, and req_in contains a new req
  if (state.active_valid == 0 && !mem_req.full() && !req_in.empty()) {
    state                        = ReaderState{};
    state.active                 = req_in.pop();
    const auto bytes_per_element = state.active.has_acc_bitwidth == 1 ? sizeof(Acc) : sizeof(Elem);
    state.total_bytes            = static_cast<std::uint32_t>(state.active.cols) * bytes_per_element;
    const auto max_bytes         = std::max(kDefaultConfig.dma_max_bytes, kDim * sizeof(Acc));
    assert_always(state.total_bytes > 0 && state.total_bytes <= max_bytes, "DmaReader request exceeds its configured row capacity");
    assert_always(state.active.cols <= kDim || state.active.block_stride > 0, "DmaReader needs a local block stride for multi-row requests");
    state.active_valid           = 1;
    state_changed                = true;
  }

  // issue next mem req for upstream read req being serviced by DmaReader 
  // (since this comes after resp handling, DmaReader can consume resp and issue next mem req in same cycle if beat was fully consumed)
  // run only when there's no mem resp outstanding, no saved beat left to consume, bytes remain, and mem-req queue has room
  if (state.active_valid == 1 && state.waiting == 0 && state.beat_bytes == 0 && state.bytes_requested < state.total_bytes && !mem_req.full()) {
    smem::MemReq req{};
    // request up to kMemBeatBytes starting at active.vaddr + bytes_requested
    state.requested_beat_bytes = static_cast<std::uint16_t>(std::min<std::size_t>(kMemBeatBytes, state.total_bytes - state.bytes_requested));
    req.addr  = state.active.vaddr + state.bytes_requested;
    req.size  = u16(state.requested_beat_bytes);
    req.write = false;
    req.id    = state.active.cmd_id;
    mem_req.push(req);
    state.waiting = 1; // mark req as oustanding until mem_resp arrives for it
    state.bytes_requested += state.requested_beat_bytes; // advance bytes asked for
    state_changed = true;
    trace("dma_reader: read addr=0x%llx bytes=%u cmd_id=%u\n",
          static_cast<unsigned long long>(req.addr), static_cast<unsigned>(req.size),
          static_cast<unsigned>(req.id));
  }

  if (pending_changed) {
    pending.count = u8(pending_count);
    pending_D_    = pending;
  }
  if (state_changed) {
    state_D_ = state;
  }
}

void DmaReader::reset() {
  pending_D_.reset(PendingRows{});
  state_D_.reset(ReaderState{});
  resp_val.reset(0);
  resp_bits.reset(DmaReadResp{});
}

} // namespace smesh
