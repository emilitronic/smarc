// **********************************************************************
// smesh/src/DmaWriter.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Jul 13 2026
/*
Store-side DMA writer skeleton implementation.
*/

#include "DmaWriter.hpp"

namespace smesh {

namespace {

// Writer currently extracts the low 64 bits since smem::MemReq still has u64 data fields
std::uint64_t low64(const StWriterData& data) {
  // TODO: remove this adapter once the external memory write packet carries a full store row.
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < data.size() && i < sizeof(value); ++i) {
    value |= static_cast<std::uint64_t>(data[i]) << (8 * i);
  }
  return value;
}

} // namespace

DmaWriter::DmaWriter(std::string /*name*/, IMPL_CTOR) {
  UPDATE(updateReady).writes(req_rdy);
  UPDATE(update).reads(req_val, req_bits).writes(mem_req);
}

void DmaWriter::updateReady() {
  req_rdy = bit(!mem_req.full());
}

void DmaWriter::update() {
  if (req_val == 0 || mem_req.full()) {
    return;
  }

  const auto writer_req = *req_bits;
  const auto issue = writer_req.issue;
  smem::MemReq req{};
  req.addr = issue.vaddr;
  req.write = true;
  req.size = writer_req.len_bytes;
  req.id = issue.cmd_id;
  req.wdata = writer_req.data_is_all_zeros ? u64(0) : u64(low64(writer_req.data));
  mem_req.push(req);
  trace("dma_writer: store vaddr=0x%llx data=0x%llx cmd_id=%u",
        static_cast<unsigned long long>(req.addr),
        static_cast<unsigned long long>(req.wdata),
        static_cast<unsigned>(req.id));
}

} // namespace smesh
