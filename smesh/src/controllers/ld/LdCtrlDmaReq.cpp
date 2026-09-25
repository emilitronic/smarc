// **********************************************************************
// smesh/src/controllers/ld/LdCtrlDmaReq.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Sep 24 2026

#include "LdCtrlDmaReq.hpp"

#include <limits>

namespace smesh {

LdCtrlDmaReq::LdCtrlDmaReq(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateWidthAndCount)
      .reads(current_localaddr, shrink, cols, actual_rows_read)
      .writes(has_acc_bitwidth, bytes_to_read);
  UPDATE(updatePayload)
      .reads(current_vaddr, current_localaddr, cols, rows, stride, all_zeros,
             scale, block_stride)
      .reads(pixel_repeat, cmd_id, has_acc_bitwidth)
      .writes(req_bits);
}

void LdCtrlDmaReq::updateWidthAndCount() {
  const bool full_width    = current_localaddr->is_acc_addr() && shrink == 0;
  const auto element_bytes = full_width ? sizeof(Acc) : sizeof(Elem);
  const auto bytes = static_cast<std::uint64_t>(*cols) *
                     static_cast<std::uint64_t>(*actual_rows_read) * element_bytes;
  assert_always(bytes <= std::numeric_limits<std::uint32_t>::max(),
                "Load tracker byte count exceeds its port width");

  has_acc_bitwidth = bit(full_width);
  bytes_to_read = static_cast<std::uint32_t>(bytes);
}

void LdCtrlDmaReq::updatePayload() {
  DmaReadReq req{};
  req.vaddr = *current_vaddr;
  req.laddr = *current_localaddr;
  req.cols = static_cast<std::uint16_t>(*cols);
  req.repeats = static_cast<std::uint16_t>(
      stride == 0 && all_zeros == 0 ? static_cast<std::uint32_t>(*rows) - 1u : 0u);
  req.block_stride = *block_stride;
  req.scale = *scale;
  req.has_acc_bitwidth = *has_acc_bitwidth;
  req.all_zeros = *all_zeros;
  req.pixel_repeats = *pixel_repeat;
  req.cmd_id = *cmd_id;
  req_bits = req;
}

void LdCtrlDmaReq::reset() {
  has_acc_bitwidth.reset(0);
  bytes_to_read.reset(0);
  req_bits.reset(DmaReadReq{});
}

} // namespace smesh
