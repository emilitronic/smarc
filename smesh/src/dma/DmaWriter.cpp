// **********************************************************************
// smesh/src/dma/DmaWriter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/* Store-side DMA writer implementation. */

#include "DmaWriter.hpp"

#include <algorithm>

namespace smesh {

DmaWriter::DmaWriter(std::string /*name*/, IMPL_CTOR) {
  active_valid_Q_ <= active_valid_D_; // is a row already partway through being sent
  active_req_Q_   <= active_req_D_;   // the row write request
  offset_Q_       <= offset_D_;
  UPDATE(updateReady).reads(active_valid_Q_).writes(req_rdy);
  UPDATE(update)
      .reads(req_val, req_bits, req_rdy, active_valid_Q_, active_req_Q_, offset_Q_, mem_req)
      .writes(mem_req, active_valid_D_, active_req_D_, offset_D_);
}

void DmaWriter::updateReady() {
  req_rdy = bit(active_valid_Q_ == 0);
}

void DmaWriter::update() {
  const bool continuing = active_valid_Q_ == 1;
  if (!continuing && (req_val == 0 || req_rdy == 0)) return;
  // Proceed if a row is in progress or a new row is being offered (val) and accepted (rdy)...
  const auto writer_req  = continuing ? *active_req_Q_ : *req_bits;
  const auto offset      = continuing ? static_cast<std::uint16_t>(*offset_Q_) : 0u;
  const auto total_bytes = static_cast<std::uint16_t>(writer_req.len_bytes); // bytes in the row
  assert_always(total_bytes > 0 && total_bytes <= writer_req.data.size(), "DmaWriter store length exceeds its row payload");
  assert_always(offset < total_bytes, "DmaWriter beat offset exceeds store length");
  if (mem_req.full()) { // o/p req FIFO is not ready to accept a new request, so stall
    if (!continuing) {  // if it's a new request, save request and initialize offset
      active_req_D_   = writer_req;
      active_valid_D_ = 1;
      offset_D_       = 0;
    }
    return;
  }
  // form the next beat and push it into outgoing memory request FIFO
  const auto beat_bytes = static_cast<std::uint16_t>(std::min<std::size_t>(kMemBeatBytes, total_bytes - offset)); // send min(DRAM byte width, bytes left to send)
  const auto issue      = writer_req.issue;
  smem::MemReq req{};
  req.addr  = static_cast<std::uint64_t>(issue.vaddr) + offset; // base addr + current byte offset
  req.write = true;
  req.size  = u16(beat_bytes);
  req.id    = issue.cmd_id;
  // pack the data payloads, copy beat_bytes from saved row into low bytes of req's data word
  std::uint64_t data = 0;
  if (writer_req.data_is_all_zeros == 0) {
    for (std::size_t byte = 0; byte < beat_bytes; ++byte) {
      // select bytes from array (right to left) and pack them in to 8-B word (right to left) (little-endian)
      data |= static_cast<std::uint64_t>(writer_req.data[offset + byte]) << (8 * byte);
    }
  }
  req.wdata = u64(data);
  mem_req.push(req); // place beat into mem request FIFO
  // track whether another beat remains after this one is queued
  const auto next_offset = static_cast<std::uint16_t>(offset + beat_bytes); // point to next byte in row after this queued beat
  const bool more_beats = next_offset < total_bytes;
  active_valid_D_ = bit(more_beats);
  offset_D_       = u16(more_beats ? next_offset : 0); // save offset for next cycle
  // save the newly acquired request so later beats use the same payload.
  if (!continuing) { active_req_D_ = writer_req;}
  trace("dma_writer: store vaddr=0x%llx bytes=%u data=0x%llx cmd_id=%u\n",
        static_cast<unsigned long long>(req.addr),
        static_cast<unsigned>(req.size),
        static_cast<unsigned long long>(req.wdata),
        static_cast<unsigned>(req.id));
}

void DmaWriter::reset() {
  req_rdy.reset(0);
  active_valid_D_.reset(0);
  active_req_D_.reset(StWriterReq{});
  offset_D_.reset(0);
}

} // namespace smesh
