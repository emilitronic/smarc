// **********************************************************************
// smesh/src/SpadWriter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-side scratchpad writer skeleton implementation.
*/

#include "SpadWriter.hpp"

namespace smesh {

namespace {
// Writer currently extracts the low 64 bits since DmaReadResp still has u64 data fields
std::uint64_t low64(const StWriterData& data) {
  // TODO: remove this adapter once the local write packet carries a full store row.
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < data.size() && i < sizeof(value); ++i) {
    value |= static_cast<std::uint64_t>(data[i]) << (8 * i);
  }
  return value;
}

} // namespace

SpadWriter::SpadWriter(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(req_rdy);
  UPDATE(update).reads(req_val, req_bits).writes(spad_write_out);
}

void SpadWriter::updateReady() {
  req_rdy = bit(!spad_write_out.full());
}

void SpadWriter::update() {
  if (req_val == 0 || spad_write_out.full()) {
    return;
  }

  const auto writer_req = *req_bits;
  const auto issue = writer_req.issue;
  DmaReadResp write{};
  write.laddr = issue.laddr;
  write.mask = 0xf;
  write.bytes_read = writer_req.len_bytes;
  write.pixel_repeats = 1;
  write.cmd_id = issue.cmd_id;
  write.last = true;
  write.data = writer_req.data_is_all_zeros ? u64(0) : u64(low64(writer_req.data));
  spad_write_out.push(write);
  trace("spad_writer: local write laddr=0x%x data=0x%llx cmd_id=%u",
        static_cast<unsigned>(write.laddr.raw),
        static_cast<unsigned long long>(write.data),
        static_cast<unsigned>(write.cmd_id));
}

} // namespace smesh
