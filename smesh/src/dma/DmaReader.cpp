// **********************************************************************
// smesh/src/dma/DmaReader.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 6 2026
/*
Minimal DMA reader implementation.
*/

#include "DmaReader.hpp"

#include <algorithm>

namespace smesh {

namespace {
constexpr std::size_t kMaxNarrowRowsPerRead = 2;
}

DmaReader::DmaReader(std::string /*name*/, IMPL_CTOR) {
  pending_Q_ <= pending_D_;
  UPDATE(updateRespView).reads(pending_Q_).writes(resp_val, resp_bits);
  UPDATE(update).reads(req_in, mem_resp, pending_Q_, resp_rdy)
      .writes(mem_req, pending_D_);
}

void DmaReader::updateRespView() {
  const auto pending = *pending_Q_;
  resp_val = bit(pending.count != 0);
  resp_bits = pending.count != 0 ? pending.first : DmaReadResp{};
}

void DmaReader::update() {
  auto pending = *pending_Q_;
  auto pending_count = static_cast<unsigned>(static_cast<std::uint8_t>(pending.count));
  bool pending_changed = false;
  if (pending_count > 0 && resp_rdy == 1) {
    pending.first = pending_count == 2 ? pending.second : DmaReadResp{};
    pending.second = DmaReadResp{};
    --pending_count;
    pending_changed = true;
  }

  // Assemble the returned beat before issuing the next one.
  if (waiting_ && !mem_resp.empty()) {
    const auto segments = active_.has_acc_bitwidth == 1
                              ? 1u : (static_cast<unsigned>(active_.cols) + kDim - 1) / kDim;
    const bool final_beat = bytes_received_ + beat_bytes_ == total_bytes_;
    if (!final_beat || pending_count + segments <= kMaxNarrowRowsPerRead) {
      const auto resp = mem_resp.pop();
      assert_always(static_cast<std::uint16_t>(resp.id) == static_cast<std::uint16_t>(active_.cmd_id),
                    "DmaReader response ID does not match active request");
      assert_always(static_cast<std::uint8_t>(resp.err) == 0,
                    "DmaReader memory response reported an error");

      const auto word = static_cast<std::uint64_t>(resp.rdata);
      for (unsigned byte = 0; byte < beat_bytes_; ++byte) {
        row_data_[bytes_received_ + byte] = static_cast<std::uint8_t>(word >> (8 * byte));
      }
      bytes_received_ += beat_bytes_;
      waiting_ = false;

      if (final_beat) {
        // The assembled row may occupy one full-width or two narrow local rows.
        assert_always(segments == 1 || active_.block_stride > 0,
                      "DmaReader needs a local block stride for multi-tile rows");
        for (unsigned segment = 0; segment < segments; ++segment) {
          const auto first_col = segment * kDim;
          const auto cols = static_cast<std::uint16_t>(
              std::min<std::size_t>(kDim, static_cast<std::uint16_t>(active_.cols) - first_col));
          const auto element_bytes = active_.has_acc_bitwidth == 1 ? sizeof(Acc) : sizeof(Elem);
          const auto chunk_bytes = static_cast<std::uint16_t>(cols * element_bytes);
          DmaReadResp dma_resp{};
          for (unsigned byte = 0; byte < chunk_bytes; ++byte) {
            dma_resp.data[byte] = row_data_[first_col * element_bytes + byte];
          }
          dma_resp.laddr = active_.laddr + segment * static_cast<std::uint16_t>(active_.block_stride);
          dma_resp.mask = u32((std::uint64_t{1} << cols) - 1u);
          dma_resp.has_acc_bitwidth = active_.has_acc_bitwidth;
          dma_resp.scale = active_.scale;
          dma_resp.repeats = active_.repeats;
          dma_resp.len = cols;
          dma_resp.bytes_read = u16(chunk_bytes);
          dma_resp.pixel_repeats = active_.pixel_repeats;
          dma_resp.cmd_id = active_.cmd_id;
          dma_resp.last = true;
          if (pending_count == 0) pending.first = dma_resp;
          else pending.second = dma_resp;
          ++pending_count;
          pending_changed = true;
        }
        active_valid_ = false;
      }
      trace("dma_reader: response data=0x%llx cmd_id=%u\n",
            static_cast<unsigned long long>(resp.rdata),
            static_cast<unsigned>(resp.id));
    }
  }

  if (pending_changed) {
    pending.count = u8(pending_count);
    pending_D_ = pending;
  }

  if (waiting_ || mem_req.full()) {
    return;
  }

  if (!active_valid_) {
    if (req_in.empty()) return;
    active_ = req_in.pop();
    const auto cols = static_cast<std::uint16_t>(active_.cols);
    const auto element_bytes = active_.has_acc_bitwidth == 1 ? sizeof(Acc) : sizeof(Elem);
    const auto row_bytes = static_cast<std::size_t>(cols) * element_bytes;
    assert_always(cols > 0 && row_bytes <= row_data_.size() &&
                      (active_.has_acc_bitwidth == 1 || cols <= kMaxNarrowRowsPerRead * kDim),
                  "DmaReader supports up to one full-width or two narrow local rows");
    total_bytes_ = static_cast<std::uint16_t>(row_bytes);
    row_data_ = {};
    bytes_requested_ = 0;
    bytes_received_ = 0;
    active_valid_ = true;
  }

  smem::MemReq req{};
  beat_bytes_ = static_cast<std::uint16_t>(
      std::min<std::size_t>(kMemBeatBytes, total_bytes_ - bytes_requested_));
  req.addr = active_.vaddr + bytes_requested_;
  req.size = u16(beat_bytes_);
  req.write = false;
  req.id = active_.cmd_id;
  mem_req.push(req);
  waiting_ = true;
  bytes_requested_ += beat_bytes_;

  trace("dma_reader: read addr=0x%llx bytes=%u cmd_id=%u\n",
        static_cast<unsigned long long>(req.addr),
        static_cast<unsigned>(req.size),
        static_cast<unsigned>(req.id));
}

void DmaReader::reset() {
  pending_D_.reset(PendingRows{});
  resp_val.reset(0);
  resp_bits.reset(DmaReadResp{});
  active_valid_ = false;
  waiting_ = false;
  active_ = {};
  row_data_ = {};
  total_bytes_ = 0;
  bytes_requested_ = 0;
  bytes_received_ = 0;
  beat_bytes_ = 0;
}

} // namespace smesh
