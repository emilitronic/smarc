// **********************************************************************
// smesh/src/dma/DmaWriter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/* Store-side DMA writer implementation. */

#include "DmaWriter.hpp"

#include <algorithm>

namespace smesh {

DmaWriter::DmaWriter(std::string /*name*/, IMPL_CTOR) {
  pool_data_Q_ <= pool_data_D_;
  active_valid_Q_ <= active_valid_D_; // is a row already partway through being sent
  active_req_Q_   <= active_req_D_;   // the row write request
  offset_Q_       <= offset_D_;
  UPDATE(updateReady).reads(active_valid_Q_).writes(req_rdy);
  UPDATE(update)
      .reads(req_val, req_bits, req_rdy, pool_data_Q_, active_valid_Q_, active_req_Q_, offset_Q_, mem_req)
      .writes(mem_req, pool_data_D_, active_valid_D_, active_req_D_, offset_D_);
}

void DmaWriter::updateReady() {
  req_rdy = bit(active_valid_Q_ == 0);
}

void DmaWriter::update() {
  const bool continuing = active_valid_Q_ == 1;
  if (!continuing && (req_val == 0 || req_rdy == 0)) return;
  // Proceed if a row is in progress or a new row is being offered (val) and accepted (rdy)...
  auto writer_req        = continuing ? *active_req_Q_ : *req_bits;
  if (!continuing) {
    // Accept one window position. The first position replaces the saved row;
    // later positions compare each channel against its saved maximum.
    if (writer_req.data_is_all_zeros == 1) writer_req.data = {};
    if (writer_req.issue.pool_en == 1) {
      assert_always(writer_req.issue.block == 0, "DmaWriter cannot combine pooling and block-moveout");
      assert_always(writer_req.data_is_full_width == 0, "DmaWriter pooling requires narrow elements");
      static_assert(sizeof(Elem) == 1, "DmaWriter pooling currently compares signed 8-bit elements");
      auto pooled = *pool_data_Q_;
      for (std::size_t lane = 0; lane < kDim; ++lane) {
        const auto incoming_byte = writer_req.data[lane];
        const auto saved_byte = pooled[lane];
        const int incoming = incoming_byte <= 127 ? incoming_byte : int(incoming_byte) - 256;
        const int saved = saved_byte <= 127 ? saved_byte : int(saved_byte) - 256;
        if (incoming > saved) pooled[lane] = incoming_byte;
      }
      writer_req.data = pooled;
      // A zero padding contribution may leave nonzero saved maxima.
      writer_req.data_is_all_zeros = 0;
    }
    pool_data_D_ = writer_req.data;
    trace("dma_writer: accept row pool_en=%u store_en=%u cmd_id=%u\n",
          unsigned(writer_req.issue.pool_en), unsigned(writer_req.issue.store_en),
          unsigned(writer_req.issue.cmd_id));
    // Intermediate pooling positions update maxima without sending any beats.
    if (writer_req.issue.store_en == 0) return;
  }
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
  pool_data_D_.reset(StWriterData{});
  req_rdy.reset(0);
  active_valid_D_.reset(0);
  active_req_D_.reset(StWriterReq{});
  offset_D_.reset(0);
}

} // namespace smesh
