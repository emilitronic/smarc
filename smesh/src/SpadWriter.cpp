// **********************************************************************
// smesh/src/SpadWriter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-side scratchpad writer skeleton implementation.
*/

#include "SpadWriter.hpp"

namespace smesh {

SpadWriter::SpadWriter(std::string /*name*/, IMPL_CTOR) {
  UPDATE(update).reads(issue_in, spad_data_in, acc_data_in).writes(spad_write_out);
}

void SpadWriter::update() {
  if (issue_in.empty() || spad_write_out.full()) {
    return;
  }

  const auto issue = issue_in.peek();
  const auto laddr = issue.laddr;
  if (laddr.is_garbage()) {
    issue_in.pop();
    trace("spad_writer: dropped garbage store cmd_id=%u", static_cast<unsigned>(issue.cmd_id));
    return;
  }

  DmaReadResp write{};
  write.laddr = laddr;
  write.mask = 0xf;
  write.bytes_read = 8;
  write.pixel_repeats = 1;
  write.cmd_id = issue.cmd_id;
  write.last = true;

  if (laddr.is_acc_addr()) {
    if (acc_data_in.empty()) {
      return;
    }
    const auto data = acc_data_in.pop();
    issue_in.pop();
    write.data = data.data;
    spad_write_out.push(write);
    trace("spad_writer: acc data to local write laddr=0x%x data=0x%llx cmd_id=%u",
          static_cast<unsigned>(write.laddr.raw),
          static_cast<unsigned long long>(write.data),
          static_cast<unsigned>(write.cmd_id));
    return;
  }

  if (spad_data_in.empty()) {
    return;
  }
  const auto data = spad_data_in.pop();
  issue_in.pop();
  write.data = data.data;
  spad_write_out.push(write);
  trace("spad_writer: spad data to local write laddr=0x%x data=0x%llx cmd_id=%u",
        static_cast<unsigned>(write.laddr.raw),
        static_cast<unsigned long long>(write.data),
        static_cast<unsigned>(write.cmd_id));
}

} // namespace smesh
